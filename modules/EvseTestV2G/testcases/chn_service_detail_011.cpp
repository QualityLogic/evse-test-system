// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_service_detail_011.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_service_detail_011 {

#pragma region COMMON

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    // If this is a state 'B' transition, cache the current time for validation
    if (event.bsp_event == types::board_support_common::Event::B) {
        cp_state_b_time = event.timestamp;
    }
}

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto test_data = &conn->ctx->test_data;

    if (not received_service_detail) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated ServiceDetail");
        return;
    }

    if (service_detail_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during ServiceDetail");
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate connection");
        return;
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'ServiceDetailReq' timer expired. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // If the `cp_state_b_time` references a point in time earlier than `service_detail_req_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < service_detail_req_time) {
            // FAIL => EV did not transition to control pilot state B
            test_data->errors.emplace_back("EV never signaled control pilot state B");
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
        }
        // Since the `cp_state_b_time` references a point in time later than `service_detail_req_time`,
        // then the control pilot signal did transition to state 'B'. We must validate that it occurred
        // within an appropriate amount of time.
        else {
            const auto cp_state_b_duration = duration_cast<milliseconds>(cp_state_b_time - service_detail_req_time);
            const auto cp_state_b_min_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL);
            const auto cp_state_b_max_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL +
                                                              V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);

            // Was state 'B' signaled before the 'V2G_Msg_Timeout' timer expired?
            if (cp_state_b_duration < cp_state_b_min_duration) {
                // The EV did not wait long enough for the CurrentDemandRes response
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV signaled Control Pilot state 'B' before the 'V2G_Msg_Timeout' "
                                               "timer expired");

                report_bsp_event(conn, {types::board_support_common::Event::B}, {
                    .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                });
            } else {
                // Was state 'B' signaled after the 'par_EVCC_StateB_Shutdown_Timeout' timer expired?
                if (cp_state_b_max_duration < cp_state_b_duration) {
                    // The EV waited too long to signal Control Pilot state 'B'
                    test_data->outcome = TestOutcome::PassCriteriaNotMet;
                    test_data->errors.emplace_back("EV signaled Control Pilot state 'B' after the "
                                                   "'par_EVCC_StateB_Shutdown_Timeout' timer expired");
                }

                report_bsp_event(conn, {types::board_support_common::Event::B}, {
                    .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                    .duration = static_cast<int>((cp_state_b_duration - cp_state_b_min_duration).count()),
                    .max_duration = V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT,
                });
            }
        }
    }

    // Was a SessionStopReq message received?
    if (session_stop_req_time.time_since_epoch().count() > 0) {

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - service_detail_req_time);
        const auto session_stop_min_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL);
        const auto session_stop_max_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL +
                                                            V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        // Was the SessionStopReq sent before Control Pilot transitioned to state 'B'?
        if (validate_control_pilot and service_detail_req_time < cp_state_b_time) {
            if (session_stop_req_time < cp_state_b_time) {
                test_data->errors.emplace_back("EV sent a SessionStopReq before Control Pilot state 'B'");
            }
        }

        // Was the SessionStopReq sent before the 'V2G_Msg_Timeout' timer expired?
        if (session_stop_duration < session_stop_min_duration) {
            // The EV did not wait long enough for the ServiceDetailRes response
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV sent a SessionStopReq before the 'V2G_Msg_Timeout' timer expired");
        }

        // Was the SessionStopReq sent after the 'par_CMN_TCP_Connection_Termination_Timeout' timer expired?
        if (session_stop_max_duration < session_stop_duration) {
            // The EV waited too long to send a SessionStopReq
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV sent a SessionStopReq after the "
                                           "'par_CMN_TCP_Connection_Termination_Timeout' timer expired");
        }
    }

    // Was the connection closed before the 'V2G_Msg_Timeout' timer expired?
    if (event.timestamp < service_detail_req_time + milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL)) {
        // The EV did not wait long enough for the ServiceDetailRes response before terminating the connection
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV terminated the connection before the 'V2G_Msg_Timeout' timer expired");

        report_connection_closed(conn, {
            .timestamp = timepoint_to_iso8601_str(event.timestamp),
        });
    } else {
        const auto tcp_close_timer_start = (session_stop_res_time.time_since_epoch().count() == 0)
            ? service_detail_req_time + milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL)
            : session_stop_res_time;
        const auto tcp_close_timer_duration = duration_cast<milliseconds>(event.timestamp - tcp_close_timer_start);
        const auto tcp_close_timer_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        if (tcp_close_timer_max_duration < tcp_close_timer_duration) {
            // The EV waited too long to terminate the connection
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV did not terminate the connection before the "
                                           "'par_CMN_TCP_Connection_Termination_Timeout' timer expired");
        } else if (test_data->outcome != TestOutcome::PassCriteriaNotMet) {
            test_data->outcome = TestOutcome::PassCriteriaMet;
        }

        report_connection_closed(conn, {
            .timestamp = timepoint_to_iso8601_str(event.timestamp),
            .duration = static_cast<int>(tcp_close_timer_duration.count()),
            .max_duration = static_cast<int>(tcp_close_timer_max_duration.count()),
        });
    }
}

#pragma endregion COMMON

#pragma region ISO_15118_2

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through ServiceDetail and allow SessionStop
    if (request_type <= V2G_SERVICE_DETAIL_MSG or request_type == V2G_SESSION_STOP_MSG)
        return Iso2Test::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);

    // The ServiceDetail message exchange is optional in ISO 15118-2. If the EV decided
    // to skip service detail and go directly to PaymentServiceSelection, that's technically
    // not an "unexpected" request. We'll only report it as an error if the service detail
    // exchange did previously take place. Otherwise, we'll just report it as preconditions
    // not met due to service detail never occurring (in on_connection_close_event).

    if (received_service_detail) {
        received_extra_request = true;
        conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
        conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    }

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_service_discovery(v2g_connection* conn) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.ServiceDiscoveryReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.ServiceDiscoveryRes;

    // Allow the ServiceDiscoveryReq to be handled normally to detect any problems
    const auto next_event = Iso2Test::handle_iso_service_discovery(conn);

    // Only modify the ServiceDiscoveryRes if no other problems occurred
    if (conn->ctx->test_data.include_custom_service and
        next_event == V2G_EVENT_NO_EVENT and
        res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Since the ServiceDetailReq is optional in ISO 15118-2, we'll add
        // a CustomService to the list of services to hopefully make the EV
        // curious enough to send a ServiceDetailReq for it.

        // Create the new Service
        iso2_ServiceType custom_service{};

        // Define a ServiceID
        custom_service.ServiceID = SERVICE_DETAIL_011_SERVICE_ID; // 60001 – 65535 allowed for implementation specific use

        // Apply a ServiceName
        constexpr char service_name[] = "Banana";
        strcpy(custom_service.ServiceName.characters, service_name);
        custom_service.ServiceName.charactersLen = strlen(service_name);
        custom_service.ServiceName_isUsed = static_cast<unsigned int>(1);

        // Apply a ServiceScope if necessary
        if (req->ServiceScope_isUsed) {
            strcpy(custom_service.ServiceScope.characters, req->ServiceScope.characters);
            custom_service.ServiceScope.charactersLen = req->ServiceScope.charactersLen;
            custom_service.ServiceScope_isUsed = true;
        }

        // Apply a ServiceCategory if necessary
        if (req->ServiceCategory_isUsed) {
            custom_service.ServiceCategory = req->ServiceCategory;
        } else {
            custom_service.ServiceCategory = iso2_serviceCategoryType_OtherCustom;
        }

        // Let's make this service free for all!
        custom_service.FreeService = static_cast<unsigned int>(1);

        // Attach our custom service to the response
        if (!res->ServiceList_isUsed) {
            res->ServiceList.Service.array[0] = custom_service;
            res->ServiceList.Service.arrayLen = 1;
            res->ServiceList_isUsed = static_cast<unsigned int>(1);
        }
        else if (res->ServiceList.Service.arrayLen < iso2_ServiceType_8_ARRAY_SIZE) {
            // (only if there's enough room in the array for an additional service)
            res->ServiceList.Service.array[res->ServiceList.Service.arrayLen++] = custom_service;
        }
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_service_detail(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.ServiceDetailReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.ServiceDetailRes;
    auto next_event = V2G_EVENT_NO_EVENT;

    // The current time is important for determining ServiceDetailReq message timeouts
    service_detail_req_time = system_clock::now();
    received_service_detail = true;

    // Publish the received EV request message to the MTT interface
    publish_iso_service_detail_req(req);

    report_iso2_request(conn, conn->ctx->current_v2g_msg, {
        .metadata = {
            .timestamp = timepoint_to_iso8601_str(service_detail_req_time),
        },
    });

    res->ResponseCode = iso2_responseCodeType_OK;

    // ServiceID reported back always matches the requested one
    res->ServiceID = req->ServiceID;

    bool service_id_found = false;

    for (uint16_t idx = 0; idx < conn->ctx->evse_v2g_data.evse_service_list_len; idx++) {
        if (req->ServiceID == conn->ctx->evse_v2g_data.evse_service_list[idx].ServiceID) {
            service_id_found = true;

            // Fill parameter list of the requested service id [V2G2-549]
            for (uint16_t idx2 = 0; idx2 < conn->ctx->evse_v2g_data.service_parameter_list[idx].ParameterSet.arrayLen; idx2++) {
                res->ServiceParameterList.ParameterSet.array[idx2] =
                    conn->ctx->evse_v2g_data.service_parameter_list[idx].ParameterSet.array[idx2];
            }
            res->ServiceParameterList.ParameterSet.arrayLen =
                conn->ctx->evse_v2g_data.service_parameter_list[idx].ParameterSet.arrayLen;
            res->ServiceParameterList_isUsed = res->ServiceParameterList.ParameterSet.arrayLen != 0 ? 1 : 0;
        }
    }
    service_id_found = (req->ServiceID == V2G_SERVICE_ID_CHARGING) ? true : service_id_found;

    if (false == service_id_found and req->ServiceID == SERVICE_DETAIL_011_SERVICE_ID) {
        service_id_found = true;
    }

    if (false == service_id_found) {
        res->ResponseCode = iso2_responseCodeType_FAILED_ServiceIDInvalid;
    }

    // Check the current response code and check if no external error has occurred
    next_event = iso_validate_response_code(&res->ResponseCode, conn);

    // Set next expected req msg
    conn->ctx->state = static_cast<int>(iso_dc_state_id::WAIT_FOR_SVCDETAIL_PAYMENTSVCSEL);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            if (conn->ctx->test_data.cp_state != types::board_support_common::Event::B) {
                const auto state_name = types::board_support_common::event_to_string(conn->ctx->test_data.cp_state);
                dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
                     state_name.c_str());
                validate_control_pilot = true;
            }
        }

        const auto timeout_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL);
        const auto elapsed_duration = system_clock::now() - service_detail_req_time;

        if (timeout_duration < elapsed_duration) {
            dlog(DLOG_LEVEL_WARNING, "Response message (type ServiceDetailRes) not configured within %d ms (took %d ms)",
                 V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL, duration_cast<milliseconds>(elapsed_duration).count());
        }

        // The 'V2G_EVENT_IGNORE_MSG' event indicates that the EVSE should not send any response
        // to the EV and simply wait for the next request or connection termination. This avoids
        // sending a late (but otherwise valid) reply to EVs with relaxed timeout settings.
        next_event = V2G_EVENT_IGNORE_MSG;
    }
    else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling ServiceDetailReq");

        service_detail_failed = true;
        conn->ctx->test_data.errors.emplace_back("Problems handling ServiceDetailReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_iso2_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
            });
        }
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination.

    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    // Event reporting is only relevant if the test was actually carried out (current demand was reached)
    if (received_service_detail) {
        // The current time is important for determining ServiceDetail message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - service_detail_req_time);
        const auto session_stop_min_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL);
        const auto session_stop_max_duration = milliseconds(V2G_TEST_MESSAGE_TIMEOUT_SERVICE_DETAIL +
                                                            V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        report_iso2_request(conn, conn->ctx->current_v2g_msg, {
            .metadata {
                .timestamp = timepoint_to_iso8601_str(session_stop_req_time),
                .duration = static_cast<int>(session_stop_duration.count()),
                .min_duration = static_cast<int>(session_stop_min_duration.count()),
                .max_duration = static_cast<int>(session_stop_max_duration.count()),
            },
        });

        // Allow the SessionStopReq to be handled normally
        next_event = Iso2Test::handle_iso_session_stop(conn);

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // The current time is important for determining TCP termination timeouts
            session_stop_res_time = system_clock::now();

            report_iso2_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
                .metadata = {
                    .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
                },
            });
        }
    } else {
        // Allow the SessionStopReq to be handled normally
        next_event = Iso2Test::handle_iso_session_stop(conn);
    }

    return next_event;
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_service_detail_011
