// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_current_demand_002.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_current_demand_002 {

inline bool is_cp_state_b(const types::board_support_common::Event& event) {
    return event == types::board_support_common::Event::B;
}

void open_contactors(const v2g_connection* conn) {
    conn->ctx->session.is_charging = false;

    if (conn->ctx->is_dc_charger == false) {
        conn->ctx->p_charger->publish_ac_open_contactor(nullptr);
    } else {
        conn->ctx->p_charger->publish_current_demand_finished(nullptr);
        conn->ctx->p_charger->publish_dc_open_contactor(nullptr);
    }
}

//=============================================
//             Event Handling
//=============================================

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    if (validate_control_pilot) {
        if (is_cp_state_b(event.bsp_event)) {
            cp_state_b_time = event.timestamp;
        }
    }
}

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    // Ensure the EVSE is no longer attempting to charge the EV
    open_contactors(conn);

    const auto event_timestamp = timepoint_to_iso8601_str(event.timestamp);
    const auto test_data = &(conn->ctx->test_data);

    // TCP closure begins either after the last CurrentDemandRes or SessionStopRes (whichever occurred most recently)
    const auto tcp_close_start = std::max(session_stop_res_time, current_demand_res_time);
    const auto tcp_close_elapsed = duration_cast<milliseconds>(event.timestamp - tcp_close_start);

    if (not current_demand_started) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated CurrentDemand");
        return;
    }

    if (current_demand_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during CurrentDemand");
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate connection");
        return;
    }

    // If the time since epoch is '0' then the value was never set.
    if (current_demand_res_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to send CurrentDemandRes");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_current_demand_002 timers unset!");
        return;
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'CurrentDemandRes' was sent. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // If the `cp_state_b_time` references a point in time earlier than `current_demand_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < current_demand_res_time) {
            // FAIL => EV did not transition to control pilot state B
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV never signaled control pilot state B");
        }
        // Since the `cp_state_b_time` references a point in time later than `current_demand_res_time`,
        // then the control pilot signal did transition to state 'B' and we must validate that it occurred
        // within an appropriate amount of time.
        else {
            const auto cp_state_b_duration = cp_state_b_time - current_demand_res_time;
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
            // 'par_EVCC_StateB_Shutdown_Timeout' of the 'CurrentDemandRes' response.
            if (cp_state_b_milliseconds > V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT) {
                // FAIL => EV took too long to transition to control pilot state B
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV took too long to signal control pilot state B");
            }
        }
    }

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the last 'CurrentDemandRes' or 'SessionStopRes' response.

    report_connection_closed(conn, {
        .timestamp = event_timestamp,
        .duration = static_cast<int>(tcp_close_elapsed.count()),
        .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
    });

    if (tcp_close_elapsed > milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
    } else if (test_data->outcome == TestOutcome::PreconditionsNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }
}

#pragma region DIN_70121

//=============================================
//             Request Handling
//=============================================

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through the first CurrentDemandReq and allow SessionStopReq
    if ((request_type <= V2G_CURRENT_DEMAND_MSG and not current_demand_started) or
        (request_type == V2G_SESSION_STOP_MSG))
        return DinTest::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_current_demand(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.CurrentDemandRes;

    report_din_request(conn, conn->ctx->current_v2g_msg);
    current_demand_started = true;

    // Handle the request normally to make sure the request was valid
    const auto next_event = DinTest::handle_din_current_demand(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Override original response code
        res->ResponseCode = din_responseCodeType_FAILED;

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            if (not is_cp_state_b(conn->ctx->test_data.cp_state)) {
                const auto state_name =
                    types::board_support_common::event_to_string(conn->ctx->test_data.cp_state);
                dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
                     state_name.c_str());
                validate_control_pilot = true;
            }
        }

        current_demand_res_time = std::chrono::system_clock::now();

        report_din_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(current_demand_res_time),
            }
        });

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CurrentDemandReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_din_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling CurrentDemandReq");
        current_demand_failed = true;
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination.

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    if (current_demand_started) {
        report_din_request(conn, conn->ctx->current_v2g_msg);

        // Allow the SessionStopReq to be handled normally
        next_event = DinTest::handle_din_session_stop(conn);

        if (next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // This time is important for determining TCP termination duration
            session_stop_res_time = std::chrono::system_clock::now();

            report_din_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
                .metadata = {
                    .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
                }
            });
        }
    } else {
        // Allow the SessionStopReq to be handled normally
        next_event = DinTest::handle_din_session_stop(conn);
    }

    return next_event;
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

//=============================================
//             Request Handling
//=============================================

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    if (conn->ctx->is_dc_charger == false) {
        // This test case is not compatible with AC charging. We'll abort the test to avoid
        // fully charging the EV through the ChargingStatus loop.

        if (not received_extra_request) {
            dlog(DLOG_LEVEL_WARNING, "DC-only test invoked as AC - stopping test");

            received_extra_request = true;
            conn->ctx->test_data.errors.emplace_back("AC charger does not support DC-only test");
            conn->ctx->stop_hlc = true;
        }

        return Iso2Test::handle_request(conn);
    }

    // Allow the communication to continue through the first CurrentDemandReq and allow SessionStopReq
    if ((request_type <= V2G_CURRENT_DEMAND_MSG and not current_demand_started) or
        (request_type == V2G_SESSION_STOP_MSG)) {

        return Iso2Test::handle_request(conn);
    }

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_current_demand(v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.CurrentDemandRes;

    report_iso2_request(conn, conn->ctx->current_v2g_msg);
    current_demand_started = true;

    // Handle the request normally to make sure the request was valid
    const auto next_event = Iso2Test::handle_iso_current_demand(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Override original response code
        res->ResponseCode = iso2_responseCodeType_FAILED;

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            if (not is_cp_state_b(conn->ctx->test_data.cp_state)) {
                const auto state_name =
                    types::board_support_common::event_to_string(conn->ctx->test_data.cp_state);
                dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
                     state_name.c_str());
                validate_control_pilot = true;
            }
        }

        current_demand_res_time = std::chrono::system_clock::now();

        report_iso2_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(current_demand_res_time),
            }
        });

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CurrentDemandReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_iso2_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling CurrentDemandReq");
        current_demand_failed = true;
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination.

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    if (current_demand_started) {
        report_iso2_request(conn, conn->ctx->current_v2g_msg);

        // Allow the SessionStopReq to be handled normally
        next_event = Iso2Test::handle_iso_session_stop(conn);

        if (next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // This time is important for determining TCP termination duration
            session_stop_res_time = std::chrono::system_clock::now();

            report_iso2_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
                .metadata = {
                    .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
                }
            });
        }
    } else {
        // Allow the SessionStopReq to be handled normally
        next_event = Iso2Test::handle_iso_session_stop(conn);
    }

    return next_event;
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_current_demand_002
