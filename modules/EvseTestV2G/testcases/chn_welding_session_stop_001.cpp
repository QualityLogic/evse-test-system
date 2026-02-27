// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_welding_session_stop_001.hpp"

#include "log.hpp"
#include "report_tools.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_welding_session_stop_001 {

#pragma region COMMON

//=============================================
//             Event Handling
//=============================================

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    if (validate_control_pilot_b and event.bsp_event == types::board_support_common::Event::B) {
        cp_state_b_time = event.timestamp;
    }
}

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto test_data = &conn->ctx->test_data;

    // It is a precondition that the EV started charging
    if (not charging_start_time) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never started charging");
        return;
    }

    // It is a precondition that charging stops gracefully
    if (forcefully_stop_charging) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV failed to stop charging");
        return;
    }

    // It is a precondition that charging stopped with a PowerDeliveryReq
    if (power_delivery_res_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Charging ended without a PowerDeliveryReq");
        return;
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'PowerDeliveryRes' was sent. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot_b) {
        // If the `cp_state_b_time` references a point in time earlier than `power_delivery_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < power_delivery_res_time) {
            // (FAIL) EV did not transition to control pilot state 'B'
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV did not signal control pilot state B");
        } else {
            // Since the `cp_state_b_time` references a point in time later than `power_delivery_res_time`,
            // then the control pilot signal did transition to state 'B' and we must validate that it occurred
            // within an appropriate amount of time.
            const auto cp_state_b_duration = cp_state_b_time - power_delivery_res_time;
            const auto cp_state_b_milliseconds = duration_cast<milliseconds>(cp_state_b_duration).count();

            constexpr types::board_support_common::BspEvent bsp_event{
                .event = types::board_support_common::Event::B,
            };

            report_bsp_event(conn, bsp_event, {
                .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                .duration = static_cast<int>(cp_state_b_milliseconds),
                .max_duration = V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT,
            });

            // The EV is required to transition to control pilot state 'B' within
            // 'par_EVCC_StateB_Shutdown_Timeout' of the 'PowerDeliveryRes' response.
            if (cp_state_b_milliseconds > V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT) {
                // (FAIL) EV did not transition to control pilot state 'B'
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV took too long to signal control pilot state B");
            }
        }
    }

    // The presence of a "FAILED" response code prevents the test from passing
    if (failed_response_message) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("Communication ended with a \"FAILED\" response");
    }
    // Check that a SessionStopRes was sent prior to communication termination
    else if (session_stop_res_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("Communication ended without a SessionStopRes");
    }
    // Check that the connection was terminated before the timer expired
    else {
        const auto tcp_close_elapsed = event.timestamp - session_stop_res_time;
        const auto tcp_close_milliseconds = duration_cast<milliseconds>(tcp_close_elapsed).count();

        report_connection_closed(conn, {
            .timestamp = timepoint_to_iso8601_str(event.timestamp),
            .duration = static_cast<int>(tcp_close_milliseconds),
            .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
        });

        if (tcp_close_milliseconds <= V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT) {
            if (test_data->outcome == TestOutcome::PreconditionsNotMet) {
                test_data->outcome = TestOutcome::PassCriteriaMet;
            }
        } else {
            dlog(DLOG_LEVEL_INFO, "EV exceeded TCP close time by %d ms",
                 tcp_close_milliseconds - V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV did not terminate communication in time");
        }
    }
}

void TestBase::update_charging_timers(const v2g_connection* conn) {
    const auto now = getmonotonictime();

    // Set the `charging_start_time` if this is the first CurrentDemandReq received
    if (charging_start_time == 0LL) {
        charging_start_time = now;
    }

    // The total amount of time the EV has been charging for
    const auto charging_elapsed_time = std::chrono::milliseconds(now - charging_start_time);

    // Gracefully stop charging after a set period of time
    if (charging_elapsed_time >= MAXIMUM_CHARGING_DURATION) {

        if (not gracefully_stop_charging) {
            gracefully_stop_charging = true;

            conn->ctx->evse_v2g_data.evse_notification = iso2_EVSENotificationType_StopCharging;
            memset(conn->ctx->evse_v2g_data.evse_status_code, iso2_DC_EVSEStatusCodeType_EVSE_Shutdown,
                   sizeof(conn->ctx->evse_v2g_data.evse_status_code));

            dlog(DLOG_LEVEL_INFO, "Gracefully stopping charging after %d seconds to proceed with the test.",
                 std::chrono::duration_cast<std::chrono::seconds>(charging_elapsed_time).count());
        }

        // Forcefully stop charging if the EV does not gracefully stop charging within a short period of time
        if (charging_elapsed_time >= MAXIMUM_CHARGING_DURATION + MAXIMUM_STOP_CHARGING_DURATION) {
            forcefully_stop_charging = true;
            conn->ctx->stop_hlc = true;

            dlog(DLOG_LEVEL_INFO, "EV never stopped trying to charge. Forcefully stopping charging.");
        }
    }
}

void TestBase::require_cp_state_b(const v2g_connection* conn) {
    // If the current control pilot state is not 'B', we must validate that it transitions
    std::lock_guard lock(conn->ctx->test_data.test_mutex);
    const auto cp_state = conn->ctx->test_data.cp_state;

    if (cp_state != types::board_support_common::Event::B) {
        validate_control_pilot_b = true;

        const auto state_name = types::board_support_common::event_to_string(cp_state);
        dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
             state_name.c_str());
    }
}

#pragma endregion COMMON

#pragma region DIN_70121

static void report_din_PowerDeliveryRes(const v2g_connection* conn, const std::chrono::system_clock::time_point& tp) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.PowerDeliveryRes;
    std::vector<types::test_report::MessageField> message_fields;

    if (res->DC_EVSEStatus_isUsed) {
        const auto evse_notification = din_EVSENotificationType_to_string(res->DC_EVSEStatus.EVSENotification);
        const auto evse_status_code = din_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode);

        message_fields.push_back({"EVSENotification", evse_notification});
        message_fields.push_back({"EVSEStatusCode", evse_status_code});
    }

    report_din_response(conn, V2G_POWER_DELIVERY_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
        .metadata = {
            .timestamp = timepoint_to_iso8601_str(tp),
        },
    });
}

v2g_event DinTestServer::din_validate_response_code(din_responseCodeType* din_response_code,
                                                    const v2g_connection* conn) {
    const auto next_event = DinTest::din_validate_response_code(din_response_code, conn);

    // A FAILED response code indicates a communication failure which problem that prevents the test from passing
    if (*din_response_code >= din_responseCodeType_FAILED) {
        failed_response_message = true;
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_current_demand(v2g_connection* conn) {
    update_charging_timers(conn);
    return DinTest::handle_din_current_demand(conn);
}

v2g_event DinTestServer::handle_din_power_delivery(v2g_connection* conn) {
    // Logs the presence of a PowerDeliveryReq requesting to stop charging

    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.PowerDeliveryReq;
    const auto stop_charging = req->ReadyToChargeState == 0;

    if (stop_charging) {
        // Log that a PowerDeliveryReq was received that requests to stop charging
        report_din_request(conn, V2G_POWER_DELIVERY_MSG, {
            .message_fields = {
                {"ReadyToChargeState", "false"},
            }
        });
    }

    // Handle the PowerDeliveryReq normally
    const auto next_event = DinTest::handle_din_power_delivery(conn);

    if (stop_charging and next_event == V2G_EVENT_NO_EVENT) {
        power_delivery_res_time = std::chrono::system_clock::now();
        report_din_PowerDeliveryRes(conn, power_delivery_res_time);
        require_cp_state_b(conn);
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;

    report_din_request(conn, V2G_SESSION_STOP_MSG);

    // Allow the SessionStopReq to be handled normally
    auto next_event = DinTest::handle_din_session_stop(conn);

    // If the SessionStopReq was handled without error, we must intervene to prevent the EVSE from
    // closing the TCP and data-link connections in order to measure how long it takes for the EV to
    // terminate the connection on its end.
    if (next_event == V2G_EVENT_SEND_AND_TERMINATE and res->ResponseCode < din_responseCodeType_FAILED) {
        // Prevent EVSE from closing the TCP connection
        next_event = V2G_EVENT_NO_EVENT;
    }

    // Report the SessionStopRes message
    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        session_stop_res_time = std::chrono::system_clock::now();
        report_din_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
            },
        });
    }

    return next_event;
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

static void report_iso2_PowerDeliveryRes(const v2g_connection* conn, const std::chrono::system_clock::time_point& tp) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.PowerDeliveryRes;
    std::vector<types::test_report::MessageField> message_fields;

    if (res->AC_EVSEStatus_isUsed) {
        const auto evse_notification = iso2_EVSENotificationType_to_string(res->AC_EVSEStatus.EVSENotification);

        message_fields.push_back({"EVSENotification", evse_notification});
    }

    if (res->DC_EVSEStatus_isUsed) {
        const auto evse_notification = iso2_EVSENotificationType_to_string(res->DC_EVSEStatus.EVSENotification);
        const auto evse_status_code = iso2_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode);

        message_fields.push_back({"EVSENotification", evse_notification});
        message_fields.push_back({"EVSEStatusCode", evse_status_code});
    }

    report_iso2_response(conn, V2G_POWER_DELIVERY_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
        .metadata = {
            .timestamp = timepoint_to_iso8601_str(tp),
        },
    });
}

v2g_event Iso2TestServer::iso_validate_response_code(iso2_responseCodeType* v2g_response_code,
                                                     const v2g_connection* conn) {
    const auto next_event = Iso2Test::iso_validate_response_code(v2g_response_code, conn);

    if (*v2g_response_code >= iso2_responseCodeType_FAILED) {
        failed_response_message = true;
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_charging_status(v2g_connection* conn) {
    update_charging_timers(conn);
    return Iso2Test::handle_iso_charging_status(conn);
}

v2g_event Iso2TestServer::handle_iso_current_demand(v2g_connection* conn) {
    update_charging_timers(conn);
    return Iso2Test::handle_iso_current_demand(conn);
}

v2g_event Iso2TestServer::handle_iso_power_delivery(v2g_connection* conn) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.PowerDeliveryReq;
    const auto stop_charging = req->ChargeProgress == iso2_chargeProgressType_Stop;

    if (stop_charging) {
        const auto charge_progress = iso2_chargeProgressType_to_string(req->ChargeProgress);
        report_iso2_request(conn, V2G_POWER_DELIVERY_MSG, {
            .message_fields = {
                {"ChargeProgress", charge_progress},
            },
        });
    }

    const auto next_event = Iso2Test::handle_iso_power_delivery(conn);

    if (stop_charging and next_event == V2G_EVENT_NO_EVENT) {
        power_delivery_res_time = std::chrono::system_clock::now();
        report_iso2_PowerDeliveryRes(conn, power_delivery_res_time);
        require_cp_state_b(conn);
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.SessionStopReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;

    const auto charging_session =
        req->ChargingSession == iso2_chargingSessionType_Pause ? "Pause"
      : req->ChargingSession == iso2_chargingSessionType_Terminate ? "Terminate"
      : "Unknown";

    report_iso2_request(conn, V2G_SESSION_STOP_MSG, {
        .message_fields = {
            {"ChargingSession", charging_session},
        },
    });

    // Allow the SessionStopReq to be handled normally
    auto next_event = Iso2Test::handle_iso_session_stop(conn);

    // If the SessionStopReq was handled without error, we must intervene to prevent the EVSE from
    // closing the TCP and data-link connections in order to measure how long it takes for the EV to
    // terminate the connection on its end.
    if (next_event == V2G_EVENT_SEND_AND_TERMINATE and res->ResponseCode < iso2_responseCodeType_FAILED) {
        // Prevent EVSE from closing the TCP connection
        next_event = V2G_EVENT_NO_EVENT;
    }

    // Report the SessionStopRes message
    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        session_stop_res_time = std::chrono::system_clock::now();

        report_iso2_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
            },
        });
    }

    return next_event;
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_welding_session_stop_001
