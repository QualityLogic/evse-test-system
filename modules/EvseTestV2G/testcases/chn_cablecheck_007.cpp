// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_cablecheck_007.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_cablecheck_007 {

inline bool is_cp_state_b(const types::board_support_common::Event& event) {
    return event == types::board_support_common::Event::B;
}

//=============================================
//             Event Handling
//=============================================

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    if (validate_control_pilot) {
        if (is_cp_state_b(event.bsp_event)) {
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

    const auto event_timestamp = timepoint_to_iso8601_str(event.timestamp);
    const auto tcp_close_start = session_stop_res_time.time_since_epoch().count() > 0 ? session_stop_res_time : payment_selection_res_time;
    const auto elapsed = duration_cast<milliseconds>(event.timestamp - tcp_close_start).count();
    const auto test_data = &(conn->ctx->test_data);

    if (not received_cable_check) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated CableCheck");
        return;
    }

    if (cable_check_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during CableCheck");
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate communication");
        return;
    }

    // If the `payment_selection_res_time` time since epoch is '0' then the value was never set.
    if (payment_selection_res_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to send PaymentServiceSelectionRes");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_cablecheck_007::TestBase payment_selection_res_time is unset!");
        return;
    }

    // Whether we reported the connection closure (to avoid double reporting it)
    bool conn_close_reported = false;

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'PaymentServiceSelectionRes' was sent. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // Sometimes the connection is closed before the EV signals CP State 'B'. Let's wait until the end
        // of the 'par_EVCC_StateB_Shutdown_Timeout' duration for the CP State 'B' to be received.
        if (cp_state_b_time < payment_selection_res_time) {
            // The timer starts shortly after the CurrentDemandRes should have been sent (not SessionStopRes)
            const auto cp_state_b_timer_duration = duration_cast<milliseconds>(event.timestamp - payment_selection_res_time);
            const auto cp_state_b_timer_max_duration = milliseconds(V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);

            // Only wait if we are still within the allowed CP State 'B' duration
            if (cp_state_b_timer_duration < cp_state_b_timer_max_duration) {
                const auto remaining_time = cp_state_b_timer_max_duration - cp_state_b_timer_duration;
                if (wait_for_cp_state_b(remaining_time)) {
                    // Report the connection closure now so that the control pilot transition
                    // is reported afterwards to visually preserve event order.
                    report_connection_closed(conn, {
                        .timestamp = event_timestamp,
                        .duration = static_cast<int>(elapsed),
                        .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
                    });
                    conn_close_reported = true;
                }
            }
        }

        // If the `cp_state_b_time` references a point in time earlier than `payment_selection_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < payment_selection_res_time) {

            // FAIL :: EV did not transition to CP state 'B'
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV never signaled control pilot state B");

            // Despite the missing control pilot state transition, we'll still provide the context of
            // connection closure timeouts and validate that the connection terminated in time.
            report_connection_closed(conn, {
                .timestamp = event_timestamp,
                .duration = static_cast<int>(elapsed),
                .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
            });

            // TODO(cb): Should this be padded to account for network latency?
            if (elapsed > V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT) {
                test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
            }

            return;
        }

        // Since the `cp_state_b_time` references a point in time later than `payment_selection_res_time`,
        // then the control pilot signal did transition to state 'B' and we must validate that it occurred
        // within an appropriate amount of time.

        const auto cp_state_b_duration = cp_state_b_time - payment_selection_res_time;
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
        // 'par_EVCC_StateB_Shutdown_Timeout' of the 'PaymentServiceSelectionRes' response.

        // TODO(cb): Should this be padded to account for network latency?
        if (cp_state_b_milliseconds > V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT) {

            // FAIL :: EV did not transition to CP state 'B'
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV took too long to signal control pilot state B");

            // Despite the delayed control pilot state transition, we'll still provide the context of
            // connection closure timeouts and validate that the connection terminated in time.
            report_connection_closed(conn, {
                .timestamp = event_timestamp,
                .duration = static_cast<int>(elapsed),
                .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
            });

            // TODO(cb): Should this be padded to account for network latency?
            if (elapsed > V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT) {
                test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
            }

            return;
        }
    }

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the 'PaymentServiceSelectionRes' response.

    if (!conn_close_reported) {
        report_connection_closed(conn, {
            .timestamp = event_timestamp,
            .duration = static_cast<int>(elapsed),
            .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
        });
    }

    // TODO(cb): Should this be padded to account for network latency?
    if (elapsed <= V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    } else {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
    }
}

#pragma region DIN_70121

//=============================================
//             Request Handling
//=============================================

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through the first CableCheckReq
    if ((request_type <= V2G_CABLE_CHECK_MSG and not received_cable_check) or
        (request_type == V2G_SESSION_STOP_MSG))
        return DinTest::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_cable_check(v2g_connection* conn) {
    const auto exi_out = conn->exi_out.dinEXIDocument;
    const auto old_res = &exi_out->V2G_Message.Body.CableCheckRes;
    const auto new_res = &exi_out->V2G_Message.Body.ServicePaymentSelectionRes;

    report_din_request(conn, V2G_CABLE_CHECK_MSG);
    received_cable_check = true;

    // Handle the request normally to make sure the request was valid
    auto next_event = DinTest::handle_din_cable_check(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and old_res->ResponseCode < din_responseCodeType_FAILED) {
        // Unset original CableCheckRes message
        exi_out->V2G_Message.Body.CableCheckRes_isUsed = 0u;

        // Initialize new PaymentServiceSelectionRes message
        exi_out->V2G_Message.Body.ServicePaymentSelectionRes_isUsed = 1u;
        init_din_ServicePaymentSelectionResType(new_res);
        conn->ctx->current_v2g_msg = V2G_PAYMENT_SERVICE_SELECTION_MSG;

        // Build new PaymentServiceSelectionRes message
        new_res->ResponseCode = din_responseCodeType_OK;

        dlog(DLOG_LEVEL_DEBUG, "Swapped CableCheckRes for ServicePaymentSelectionRes");

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            if (not is_cp_state_b(conn->ctx->test_data.cp_state)) {
                const auto state_name = types::board_support_common::event_to_string(conn->ctx->test_data.cp_state);
                dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
                     state_name.c_str());
                validate_control_pilot = true;
            }
        }

        payment_selection_res_time = std::chrono::system_clock::now();

        report_din_response(conn, V2G_PAYMENT_SERVICE_SELECTION_MSG, {
            .response_code = new_res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(payment_selection_res_time),
            }
        });

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CableCheckReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_din_response(conn, V2G_SERVICE_DISCOVERY_MSG, {
                .response_code = old_res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling CableCheckReq");
        cable_check_failed = true;
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination.

    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    if (received_cable_check) {
        // The current time is important for determining SessionSetup message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - payment_selection_res_time);
        const auto session_stop_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        report_din_request(conn, conn->ctx->current_v2g_msg, {
            .metadata {
                .timestamp = timepoint_to_iso8601_str(session_stop_req_time),
                .duration = static_cast<int>(session_stop_duration.count()),
                .max_duration = static_cast<int>(session_stop_max_duration.count()),
            },
        });

        // Allow the SessionStopReq to be handled normally
        next_event = DinTest::handle_din_session_stop(conn);

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            // The current time is important for determining TCP termination timeouts
            session_stop_res_time = system_clock::now();

            report_din_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
                .metadata = {
                    .timestamp = timepoint_to_iso8601_str(session_stop_res_time),
                },
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

    // Allow the communication to continue normally through the first CableCheckReq
    if ((request_type <= V2G_CABLE_CHECK_MSG and not received_cable_check) or
        (request_type == V2G_SESSION_STOP_MSG))
        return Iso2Test::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_cable_check(v2g_connection* conn) {
    const auto exi_out = conn->exi_out.iso2EXIDocument;
    const auto old_res = &exi_out->V2G_Message.Body.CableCheckRes;
    const auto new_res = &exi_out->V2G_Message.Body.PaymentServiceSelectionRes;

    report_din_request(conn, V2G_CABLE_CHECK_MSG);
    received_cable_check = true;

    // Handle the request normally to make sure the request was valid
    auto next_event = Iso2Test::handle_iso_cable_check(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and old_res->ResponseCode < iso2_responseCodeType_FAILED) {
        // Unset original CableCheckRes message
        exi_out->V2G_Message.Body.CableCheckRes_isUsed = 0u;

        // Initialize new PaymentServiceSelectionRes message
        exi_out->V2G_Message.Body.PaymentServiceSelectionRes_isUsed = 1u;
        init_iso2_PaymentServiceSelectionResType(new_res);
        conn->ctx->current_v2g_msg = V2G_PAYMENT_SERVICE_SELECTION_MSG;

        // Build new PaymentServiceSelectionRes message
        new_res->ResponseCode = iso2_responseCodeType_OK;

        dlog(DLOG_LEVEL_DEBUG, "Swapped CableCheckRes for PaymentServiceSelectionRes");

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            if (not is_cp_state_b(conn->ctx->test_data.cp_state)) {
                const auto state_name = types::board_support_common::event_to_string(conn->ctx->test_data.cp_state);
                dlog(DLOG_LEVEL_INFO, "The control pilot state is '%s' instead of 'B', expecting transition...",
                     state_name.c_str());
                validate_control_pilot = true;
            }
        }

        payment_selection_res_time = std::chrono::system_clock::now();

        report_iso2_response(conn, V2G_PAYMENT_SERVICE_SELECTION_MSG, {
            .response_code = new_res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(payment_selection_res_time),
            }
        });

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling CableCheckReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_iso2_response(conn, V2G_SERVICE_DISCOVERY_MSG, {
                .response_code = old_res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling CableCheckReq");
        cable_check_failed = true;
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
    if (received_cable_check) {
        // The current time is important for determining SessionSetup message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - payment_selection_res_time);
        const auto session_stop_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        report_iso2_request(conn, conn->ctx->current_v2g_msg, {
            .metadata {
                .timestamp = timepoint_to_iso8601_str(session_stop_req_time),
                .duration = static_cast<int>(session_stop_duration.count()),
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

} // namespace testing::chn_cablecheck_007
