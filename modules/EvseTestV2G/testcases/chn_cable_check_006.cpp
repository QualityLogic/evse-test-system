// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_cable_check_006.hpp"
#include "log.hpp"
#include "report_tools.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_cable_check_006 {

#pragma region Common

bool TestBase::is_cable_check_running_forever(const std::chrono::system_clock::time_point& tp) const {
    // Max between the 'earliest' and 'latest' since the 'latest' will not be set until the second request
    const auto timer_duration = tp - std::max(earliest_cable_check_timer_start, latest_cable_check_timer_start);
    const auto timeout_duration = std::chrono::milliseconds(
        V2G_EVCC_CABLE_CHECK_TIMEOUT + V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);
    return timer_duration > timeout_duration;
}

//=============================================
//             Event Handling
//=============================================

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    // Only update control pilot timers if the test actively requested validation
    if (validate_control_pilot) {
        // Did the EV signal state 'B' over control pilot?
        if (event.bsp_event == types::board_support_common::Event::B) {
            {
                std::lock_guard lock(bsp_mutex);
                cp_state_b_time = event.timestamp;
                set_cp_state_b_time = true;
            }
            bsp_cv.notify_one();
        }
    }
}

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto test_data = &(conn->ctx->test_data);
    const auto cable_check_timeout_duration = milliseconds(V2G_EVCC_CABLE_CHECK_TIMEOUT);
    const auto cp_state_b_timeout_duration = milliseconds(V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);
    const auto tcp_close_timeout_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);
    const auto earliest_cable_check_timer_expired = earliest_cable_check_timer_start + cable_check_timeout_duration;
    const auto latest_cable_check_timer_expired = latest_cable_check_timer_start + cable_check_timeout_duration;
    const auto session_stop_req_received = last_session_stop_req_time.time_since_epoch().count() > 0;

    //=============================================
    //             State Validation
    //=============================================

    if (not cable_check_started) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated CableCheck");
        return;
    }

    if (cable_check_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during CableCheck");
        return;
    }

    if (received_extra_request or cable_check_aborted) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate communication");
        return;
    }

    // If the time since epoch is '0' then the value was never set.
    if ((last_cable_check_req_time.time_since_epoch().count() == 0) or
        (last_cable_check_res_time.time_since_epoch().count() == 0) or
        (earliest_cable_check_timer_start.time_since_epoch().count() == 0) or
        (latest_cable_check_timer_start.time_since_epoch().count() == 0)) {

        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to send CableCheckRes");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_cable_check_006 timers unset!");
        return;
    }

    //=============================================
    //             Timing Validation
    //=============================================

    // Was the last CableCheckReq sent unambiguously before the V2G_EVCC_CableCheck_Timer expired?
    if ((last_cable_check_res_time + max_cable_check_sequence_duration) < earliest_cable_check_timer_expired) {
        // Based on the maximum duration between any CableCheckRes=>CableCheckReq, the EV had enough time
        // to send a followup CableCheckReq before the earliest possible time the timer could have expired.
        const auto duration =
            earliest_cable_check_timer_expired - (last_cable_check_res_time + max_cable_check_sequence_duration);
        test_data->errors.emplace_back("EV discontinued cable check before the V2G_EVCC_CableCheck_Timer expired by"
                                       " at least %d ms", duration_cast<milliseconds>(duration).count());
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
    }

    // Was the last CableCheckReq sent unambiguously after the V2G_EVCC_CableCheck_Timer expired?
    if (latest_cable_check_timer_expired < (last_cable_check_req_time - last_cable_check_sequence_duration)) {
        // Based on the last duration between the CableCheckRes=>CableCheckReq, the EV sent a followup
        // CableCheckReq after the latest possible the time timer could have expired.
        const auto duration =
            (last_cable_check_req_time - last_cable_check_sequence_duration) - latest_cable_check_timer_expired;
        test_data->errors.emplace_back("EV continued cable check after the V2G_EVCC_CableCheck_Timer expired by"
                                       " at least %d ms", duration_cast<milliseconds>(duration).count());
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'CableCheckReq' timer expired. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // Sometimes the connection is closed before the EV signals CP State 'B'. Let's wait until the end
        // of the 'par_EVCC_StateB_Shutdown_Timeout' duration for the CP State 'B' to be received.
        if (not cp_state_b_time.time_since_epoch().count()) {
            const auto cp_state_b_timer_start = last_cable_check_res_time + max_cable_check_sequence_duration;
            const auto cp_state_b_timer_duration = duration_cast<milliseconds>(event.timestamp - cp_state_b_timer_start);
            const auto cp_state_b_timer_max_duration = milliseconds(V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);

            // Only wait if we are still within the allowed CP State 'B' duration
            if (cp_state_b_timer_duration < cp_state_b_timer_max_duration) {
                const auto remaining_time = cp_state_b_timer_max_duration - cp_state_b_timer_duration;
                wait_for_cp_state_b(remaining_time);
            }
        }

        // If the time since epoch is '0' then the control pilot signal never transitioned to state 'B'
        // before the connection closed.
        if (not cp_state_b_time.time_since_epoch().count()) {
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back(
                "EV never signaled Control Pilot state 'B' before terminating the connection");
        } else {
            // Since the control pilot signal did transition to state 'B' and we must validate that it
            // occurred within an appropriate amount of time.

            const auto latest_cp_state_b_timer_start =
                std::min(cp_state_b_time, last_cable_check_res_time + max_cable_check_sequence_duration);
            const auto latest_cp_state_b_timer_expired = latest_cp_state_b_timer_start + cp_state_b_timeout_duration;
            const auto cp_state_b_timer_duration = duration_cast<milliseconds>(cp_state_b_time - latest_cp_state_b_timer_start);

            // Did the state 'B' transition occur unambiguously after the par_EVCC_StateB_Shutdown_Timeout expired?
            if (latest_cp_state_b_timer_expired < cp_state_b_time) {
                // Based on the maximum duration between any CableCheckRes=>CableCheckReq, the EV did not signal
                // control pilot state 'B' before the last possible time the timer could have expired.
                const auto duration = cp_state_b_time - latest_cp_state_b_timer_expired;
                test_data->errors.emplace_back("EV signaled control pilot state 'B' after the "
                                               "par_EVCC_StateB_Shutdown_Timeout expired by at least %d ms",
                                               duration_cast<milliseconds>(duration).count());
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
            }

            report_bsp_event(conn, {types::board_support_common::Event::B}, {
                .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                .duration = static_cast<int>(cp_state_b_timer_duration.count()),
                .max_duration = static_cast<int>(cp_state_b_timeout_duration.count()),
            });
        }
    }

    auto latest_tcp_close_timer_start =
        std::min(event.timestamp, last_cable_check_req_time + max_cable_check_sequence_duration);

    if (session_stop_req_received) {
        // Was the last SessionStopReq sent unambiguously after the par_CMN_TCP_Connection_Termination_Timeout expired?
        if (latest_tcp_close_timer_start + tcp_close_timeout_duration < last_session_stop_req_time) {
            const auto duration =
                last_session_stop_req_time - (latest_tcp_close_timer_start + tcp_close_timeout_duration);
            test_data->errors.emplace_back("EV sent SessionStopReq after the "
                                           "par_CMN_TCP_Connection_Termination_Timeout expired by at least %d ms",
                                           duration_cast<milliseconds>(duration).count());
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
        }

        // Update latest connection termination timer start time
        if (last_session_stop_res_time.time_since_epoch().count()) {
            latest_tcp_close_timer_start =
                std::min(event.timestamp, last_session_stop_res_time + max_cable_check_sequence_duration);
        }
    }

    const auto tcp_close_timer_duration = duration_cast<milliseconds>(event.timestamp - latest_tcp_close_timer_start);

    // Was the connection terminated unambiguously after the par_CMN_TCP_Connection_Termination_Timeout expired?
    if (tcp_close_timeout_duration < tcp_close_timer_duration) {
        const auto duration = tcp_close_timer_duration - tcp_close_timeout_duration;
        test_data->errors.emplace_back("EV terminated the connection after the "
                                       "par_CMN_TCP_Connection_Termination_Timeout expired by at least %d ms",
                                       duration_cast<milliseconds>(duration).count());
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
    }
    else if (test_data->outcome != TestOutcome::PassCriteriaNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }

    report_connection_closed(conn, {
        .timestamp = timepoint_to_iso8601_str(event.timestamp),
        .duration = static_cast<int>(tcp_close_timer_duration.count()),
        .max_duration = static_cast<int>(tcp_close_timeout_duration.count()),
    });
}

#pragma endregion Common

#pragma region DIN_70121

//=============================================
//             Request Handling
//=============================================

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through CableCheck and allow SessionStop
    if (request_type <= V2G_CABLE_CHECK_MSG or request_type == V2G_SESSION_STOP_MSG)
        return DinTest::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_charge_parameter(v2g_connection* conn) {
    const auto next_event = DinTest::handle_din_charge_parameter(conn);

    if (next_event == V2G_EVENT_NO_EVENT and conn->ctx->state == din_state_id::WAIT_FOR_CABLECHECK) {
        // Update the earliest time the V2G_EVCC_CableCheck_Timer could have started
        earliest_cable_check_timer_start = std::chrono::system_clock::now();
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_cable_check(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::system_clock;
    using std::chrono::milliseconds;

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.CableCheckRes;
    const auto now = system_clock::now();

    // Is this the first CableCheckReq message we've received?
    if (conn->ctx->last_v2g_msg != V2G_CABLE_CHECK_MSG) {
        cable_check_started = true;
    }
    // Is this the second CableCheckReq message we've received?
    else if (not latest_cable_check_timer_start.time_since_epoch().count()) {
        // Update the latest time the V2G_EVCC_CableCheck_Timer could have started
        latest_cable_check_timer_start = now;
    }

    // Update the last CableCheck sequence duration if a CableCheckRes was sent previously
    if (last_cable_check_res_time.time_since_epoch().count())
        last_cable_check_sequence_duration = duration_cast<milliseconds>(now - last_cable_check_res_time);

    // Update the max CableCheck sequence duration
    if (max_cable_check_sequence_duration < last_cable_check_sequence_duration)
        max_cable_check_sequence_duration = last_cable_check_sequence_duration;

    last_cable_check_req_time = now;
    report_din_cable_check_req(conn, now);

    // Allow the CableCheckReq to be handled normally
    auto next_event = DinTest::handle_din_cable_check(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Check if the EVSE Status indicates a problem
        if ((res->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_Ready) and
            (res->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive)) {
            dlog(DLOG_LEVEL_WARNING, "EVSEStatusCode is not Ready or IsolationMonitoringActive");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSEStatusCode");
            cable_check_failed = true;
        }

        // Check if the EVSE Isolation Status indicates a problem
        else if (res->DC_EVSEStatus.EVSEIsolationStatus_isUsed and
            res->DC_EVSEStatus.EVSEIsolationStatus == din_isolationLevelType_Fault) {
            dlog(DLOG_LEVEL_WARNING, "EVSEIsolationStatus is Fault");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSEIsolationStatus");
            cable_check_failed = true;
        }

        // Check if the EVSE Notification indicates a problem
        else if (res->DC_EVSEStatus.EVSENotification != din_EVSENotificationType_None) {
            dlog(DLOG_LEVEL_WARNING, "EVSENotificationType is not None");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSENotification");
            cable_check_failed = true;
        }

        else {
            if (res->EVSEProcessing == din_EVSEProcessingType_Ongoing) {
                if (res->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive) {
                    res->DC_EVSEStatus.EVSEStatusCode = din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive;
                }
                if (not res->DC_EVSEStatus.EVSEIsolationStatus_isUsed) {
                    res->DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1;
                    res->DC_EVSEStatus.EVSEIsolationStatus = din_isolationLevelType_Invalid;
                } else if (res->DC_EVSEStatus.EVSEIsolationStatus != din_isolationLevelType_Invalid) {
                    res->DC_EVSEStatus.EVSEIsolationStatus = din_isolationLevelType_Invalid;
                }
                if (conn->ctx->state != WAIT_FOR_CABLECHECK) {
                    conn->ctx->state = WAIT_FOR_CABLECHECK;
                }
            } else if (res->EVSEProcessing == din_EVSEProcessingType_Finished) {
                res->EVSEProcessing = din_EVSEProcessingType_Ongoing;
                res->DC_EVSEStatus.EVSEStatusCode = din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive;
                res->DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1;
                res->DC_EVSEStatus.EVSEIsolationStatus = din_isolationLevelType_Invalid;
                conn->ctx->state = WAIT_FOR_CABLECHECK;
            }

            // If CableCheck has been running longer than V2G_EVCC_CableCheck_Timeout and
            // par_CMN_TCP_Connection_Termination_Timeout, then the EV has failed the test.
            if (is_cable_check_running_forever(last_cable_check_req_time)) {
                dlog(DLOG_LEVEL_WARNING, "Waiting for CableCheck forever! Aborting test case");
                if (cable_check_aborted)
                    next_event = V2G_EVENT_SEND_AND_TERMINATE;
                res->ResponseCode = din_responseCodeType_FAILED;
                cable_check_aborted = true;
            }
        }

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CableCheckReq");

        cable_check_failed = true;
        conn->ctx->test_data.errors.emplace_back("Problems handling CableCheckReq");
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            validate_control_pilot = conn->ctx->test_data.cp_state != types::board_support_common::Event::B;
        }

        // The current time is important for determining when the following timers start:
        // - par_EVCC_StateB_Shutdown_Timeout
        // - par_CMN_TCP_Connection_Termination_Timeout
        last_cable_check_res_time = system_clock::now();

        report_din_cable_check_res(conn, res, last_cable_check_res_time);
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;

    if (cable_check_started) {
        // The current time is important for determining how long the EV allowed cable check to run
        last_session_stop_req_time = std::chrono::system_clock::now();
        report_din_session_stop_req(conn, last_session_stop_req_time);
    }

    // Allow the SessionStopReq to be handled normally
    const auto next_event = DinTest::handle_din_session_stop(conn);

    if (cable_check_started) {
        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // The current time is important for determining connection termination timeouts
            last_session_stop_res_time = std::chrono::system_clock::now();
            report_din_session_stop_res(conn, res, last_session_stop_res_time);
        }
    }

    return next_event;
}

//=============================================
//             Report Handling
//=============================================

void DinTestServer::report_din_cable_check_req(const v2g_connection* conn,
                                               const std::chrono::system_clock::time_point& tp) {
    report_din_request(conn, V2G_CABLE_CHECK_MSG, {
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

void DinTestServer::report_din_cable_check_res(const v2g_connection* conn,
                                               const din_CableCheckResType* res,
                                               const std::chrono::system_clock::time_point& tp) {
    std::vector<types::test_report::MessageField> message_fields{
        {"EVSEProcessing", din_EVSEProcessingType_to_string(res->EVSEProcessing)},
        {"EVSEStatusCode", din_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode)},
    };

    if (res->DC_EVSEStatus.EVSEIsolationStatus_isUsed) {
        const auto isolationStatus = din_isolationLevelType_to_string(res->DC_EVSEStatus.EVSEIsolationStatus);
        message_fields.push_back({ "EVSEIsolationStatus", isolationStatus });
    }

    report_din_response(conn, V2G_CABLE_CHECK_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

void DinTestServer::report_din_session_stop_req(const v2g_connection* conn,
                                                const std::chrono::system_clock::time_point& tp) {
    report_din_request(conn, V2G_SESSION_STOP_MSG, {
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

void DinTestServer::report_din_session_stop_res(const v2g_connection* conn,
                                                const din_SessionStopResType* res,
                                                const std::chrono::system_clock::time_point& tp) {
    report_din_response(conn, V2G_SESSION_STOP_MSG, {
        .response_code = res->ResponseCode,
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

//=============================================
//             Request Handling
//=============================================

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // This test is only compatible with 'DC' charging. If this is an 'AC' charger,
    // we'll gracefully abort the charging session to avoid charging the EV.
    if (conn->ctx->is_dc_charger == false) {
        if (not received_extra_request) {
            dlog(DLOG_LEVEL_WARNING, "DC-only test invoked as AC - stopping test");

            received_extra_request = true;
            conn->ctx->test_data.errors.emplace_back("AC charger does not support DC-only test");
            conn->ctx->stop_hlc = true;
        }
        return Iso2Test::handle_request(conn);
    }

    // Allow the communication to continue normally through CableCheck and allow SessionStop
    if (request_type <= V2G_CABLE_CHECK_MSG or request_type == V2G_SESSION_STOP_MSG)
        return Iso2Test::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_charge_parameter_discovery(v2g_connection* conn) {
    const auto next_event = Iso2Test::handle_iso_charge_parameter_discovery(conn);

    if (next_event == V2G_EVENT_NO_EVENT and
        conn->ctx->state == static_cast<int>(iso_dc_state_id::WAIT_FOR_CABLECHECK)) {
        // Update the earliest time the V2G_EVCC_CableCheck_Timer could have started
        earliest_cable_check_timer_start = std::chrono::system_clock::now();
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_cable_check(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::system_clock;
    using std::chrono::milliseconds;

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.CableCheckRes;
    const auto now = system_clock::now();

    // Is this the first CableCheckReq message we've received?
    if (conn->ctx->last_v2g_msg != V2G_CABLE_CHECK_MSG) {
        cable_check_started = true;
    }
    // Is this the second CableCheckReq message we've received?
    else if (not latest_cable_check_timer_start.time_since_epoch().count()) {
        // Update the latest time the V2G_EVCC_CableCheck_Timer could have started
        latest_cable_check_timer_start = now;
    }

    // Update the last CableCheck sequence duration if a CableCheckRes was sent previously
    if (last_cable_check_res_time.time_since_epoch().count())
        last_cable_check_sequence_duration = duration_cast<milliseconds>(now - last_cable_check_res_time);

    // Update the max CableCheck sequence duration
    if (max_cable_check_sequence_duration < last_cable_check_sequence_duration)
        max_cable_check_sequence_duration = last_cable_check_sequence_duration;

    last_cable_check_req_time = now;
    report_iso_cable_check_req(conn, now);

    // Allow the CableCheckReq to be handled normally
    auto next_event = Iso2Test::handle_iso_cable_check(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Check if the EVSE Status indicates a problem
        if ((res->DC_EVSEStatus.EVSEStatusCode != iso2_DC_EVSEStatusCodeType_EVSE_Ready) and
            (res->DC_EVSEStatus.EVSEStatusCode != iso2_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive)) {
            dlog(DLOG_LEVEL_WARNING, "EVSEStatusCode is not Ready or IsolationMonitoringActive");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSEStatusCode");
            cable_check_failed = true;
        }

        // Check if the EVSE Isolation Status indicates a problem
        else if (res->DC_EVSEStatus.EVSEIsolationStatus_isUsed and
            res->DC_EVSEStatus.EVSEIsolationStatus == iso2_isolationLevelType_Fault) {
            dlog(DLOG_LEVEL_WARNING, "EVSEIsolationStatus is Fault");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSEIsolationStatus");
            cable_check_failed = true;
        }

        // Check if the EVSE Notification indicates a problem
        else if (res->DC_EVSEStatus.EVSENotification != iso2_EVSENotificationType_None) {
            dlog(DLOG_LEVEL_WARNING, "EVSENotificationType is not None");
            conn->ctx->test_data.errors.emplace_back("Unexpected EVSENotification");
            cable_check_failed = true;
        }

        else {
            if (res->EVSEProcessing == iso2_EVSEProcessingType_Ongoing) {
                if (res->DC_EVSEStatus.EVSEStatusCode != iso2_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive) {
                    res->DC_EVSEStatus.EVSEStatusCode = iso2_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive;
                }
                if (not res->DC_EVSEStatus.EVSEIsolationStatus_isUsed) {
                    res->DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1;
                    res->DC_EVSEStatus.EVSEIsolationStatus = iso2_isolationLevelType_Invalid;
                } else if (res->DC_EVSEStatus.EVSEIsolationStatus != iso2_isolationLevelType_Invalid) {
                    res->DC_EVSEStatus.EVSEIsolationStatus = iso2_isolationLevelType_Invalid;
                }
                if (conn->ctx->state != static_cast<int>(iso_dc_state_id::WAIT_FOR_CABLECHECK)) {
                    conn->ctx->state = static_cast<int>(iso_dc_state_id::WAIT_FOR_CABLECHECK);
                }
            }
            else if (res->EVSEProcessing == iso2_EVSEProcessingType_Finished) {
                res->EVSEProcessing = iso2_EVSEProcessingType_Ongoing;
                res->DC_EVSEStatus.EVSEStatusCode = iso2_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive;
                res->DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1;
                res->DC_EVSEStatus.EVSEIsolationStatus = iso2_isolationLevelType_Invalid;
                conn->ctx->state = static_cast<int>(iso_dc_state_id::WAIT_FOR_CABLECHECK);
            }

            // If CableCheck has been running longer than V2G_EVCC_CableCheck_Timeout and
            // par_CMN_TCP_Connection_Termination_Timeout, then the EV has failed the test.
            if (is_cable_check_running_forever(last_cable_check_req_time)) {
                dlog(DLOG_LEVEL_WARNING, "Waiting for CableCheck forever! Aborting test case");
                if (cable_check_aborted)
                    next_event = V2G_EVENT_SEND_AND_TERMINATE;
                res->ResponseCode = iso2_responseCodeType_FAILED;
                cable_check_aborted = true;
            }
        }
    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CableCheckReq");

        cable_check_failed = true;
        conn->ctx->test_data.errors.emplace_back("Problems handling CableCheckReq");
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            validate_control_pilot = conn->ctx->test_data.cp_state != types::board_support_common::Event::B;
        }

        // The current time is important for determining when the following timers start:
        // - par_EVCC_StateB_Shutdown_Timeout
        // - par_CMN_TCP_Connection_Termination_Timeout
        last_cable_check_res_time = system_clock::now();
        report_iso_cable_check_res(conn, res, last_cable_check_res_time);
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;

    if (cable_check_started) {
        // The current time is important for determining how long the EV allowed cable check to run
        last_session_stop_req_time = std::chrono::system_clock::now();
        report_iso_session_stop_req(conn, last_session_stop_req_time);
    }

    // Allow the SessionStopReq to be handled normally
    const auto next_event = Iso2Test::handle_iso_session_stop(conn);

    if (cable_check_started) {
        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // The current time is important for determining connection termination timeouts
            last_session_stop_res_time = std::chrono::system_clock::now();
            report_iso_session_stop_res(conn, res, last_session_stop_res_time);
        }
    }

    return next_event;
}

//=============================================
//             Report Handling
//=============================================

void Iso2TestServer::report_iso_cable_check_req(const v2g_connection* conn,
                                                const std::chrono::system_clock::time_point& tp) {
    report_iso2_request(conn, V2G_CABLE_CHECK_MSG, {
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

void Iso2TestServer::report_iso_cable_check_res(const v2g_connection* conn,
                                                const iso2_CableCheckResType* res,
                                                const std::chrono::system_clock::time_point& tp) {
    std::vector<types::test_report::MessageField> message_fields{
        {"EVSEProcessing", iso2_EVSEProcessingType_to_string(res->EVSEProcessing)},
        {"EVSEStatusCode", iso2_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode)},
    };

    if (res->DC_EVSEStatus.EVSEIsolationStatus_isUsed) {
        const auto isolationStatus = iso2_isolationLevelType_to_string(res->DC_EVSEStatus.EVSEIsolationStatus);
        message_fields.push_back({ "EVSEIsolationStatus", isolationStatus });
    }

    report_iso2_response(conn, V2G_CABLE_CHECK_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) },
    });
}

void Iso2TestServer::report_iso_session_stop_req(const v2g_connection* conn,
                                                 const std::chrono::system_clock::time_point& tp) {
    report_iso2_request(conn, V2G_SESSION_STOP_MSG, {
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

void Iso2TestServer::report_iso_session_stop_res(const v2g_connection* conn,
                                                 const iso2_SessionStopResType* res,
                                                 const std::chrono::system_clock::time_point& tp) {
    report_iso2_response(conn, V2G_SESSION_STOP_MSG, {
        .response_code = res->ResponseCode,
        .metadata { .timestamp = timepoint_to_iso8601_str(tp) }
    });
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_cable_check_006
