// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_service_detail_payment_selection_003.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_service_detail_payment_selection_003 {

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

    const auto payment_msg_name = get_payment_selection_msg_name();
    const auto test_data = &conn->ctx->test_data;

    if (not received_payment_selection) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back(fmt::format("EV never initiated {}", payment_msg_name));
        return;
    }

    if (payment_selection_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back(fmt::format("Problems occurred during {}", payment_msg_name));
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate connection");
        return;
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'PaymentServiceSelectionRes' was sent. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // If the `cp_state_b_time` references a point in time earlier than `payment_selection_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < payment_selection_res_time) {
            // FAIL => EV did not transition to control pilot state B
            test_data->errors.emplace_back("EV never signaled control pilot state B");
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
        }
        // Since the `cp_state_b_time` references a point in time later than `payment_selection_res_time`,
        // then the control pilot signal did transition to state 'B'. We must validate that it occurred
        // within an appropriate amount of time.
        else {
            const auto cp_state_b_duration = duration_cast<milliseconds>(cp_state_b_time - payment_selection_res_time);
            const auto cp_state_b_max_duration = milliseconds(V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);

            // Was state 'B' signaled after the 'par_EVCC_StateB_Shutdown_Timeout' timer expired?
            if (cp_state_b_max_duration < cp_state_b_duration) {
                // The EV waited too long to signal Control Pilot state 'B'
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV signaled Control Pilot state 'B' after the "
                                               "'par_EVCC_StateB_Shutdown_Timeout' timer expired");
            }

            report_bsp_event(conn, {types::board_support_common::Event::B}, {
                .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                .duration = static_cast<int>(cp_state_b_duration.count()),
                .max_duration = static_cast<int>(cp_state_b_max_duration.count()),
            });
        }
    }

    // Was a SessionStopReq message received?
    if (session_stop_req_time.time_since_epoch().count() > 0) {

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - payment_selection_res_time);
        const auto session_stop_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        // Was the SessionStopReq sent before Control Pilot transitioned to state 'B'?
        if (validate_control_pilot and payment_selection_res_time < cp_state_b_time) {
            if (session_stop_req_time < cp_state_b_time) {
                test_data->errors.emplace_back("EV sent a SessionStopReq before Control Pilot state 'B'");
            }
        }

        // Was the SessionStopReq sent after the 'par_CMN_TCP_Connection_Termination_Timeout' timer expired?
        if (session_stop_max_duration < session_stop_duration) {
            // The EV waited too long to send a SessionStopReq
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV sent a SessionStopReq after the "
                                           "'par_CMN_TCP_Connection_Termination_Timeout' timer expired");
        }
    }

    const auto tcp_close_timer_start = (session_stop_res_time.time_since_epoch().count() == 0)
            ? payment_selection_res_time
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

#pragma endregion COMMON

#pragma region DIN_70121

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through PaymentServiceSelection and allow SessionStop
    if (request_type <= V2G_PAYMENT_SERVICE_SELECTION_MSG or request_type == V2G_SESSION_STOP_MSG)
        return DinTest::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);

    received_extra_request = true;
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_service_payment_selection(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.ServicePaymentSelectionRes;

    report_din_request(conn, conn->ctx->current_v2g_msg);
    received_payment_selection = true;

    // Handle the request normally to make sure the request is valid
    const auto next_event = DinTest::handle_din_service_payment_selection(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Override original response code
        res->ResponseCode = din_responseCodeType_FAILED;

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

        // Notify board support to turn off the PWM oscillator
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_off();
        report_bsp_event(conn, {types::board_support_common::Event::PowerOff});

        payment_selection_res_time = std::chrono::system_clock::now();
        report_din_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(payment_selection_res_time),
            }
        });
    }
    else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling ServicePaymentSelectionReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_din_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling ServicePaymentSelectionReq");
        payment_selection_failed = true;
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

    // Event reporting is only relevant if the test was actually carried out (current demand was reached)
    if (received_payment_selection and not payment_selection_failed) {
        // The current time is important for determining connection termination timeouts
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

const char* DinTestServer::get_payment_selection_msg_name() {
    return "ServicePaymentSelection";
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through PaymentServiceSelection and allow SessionStop
    if (request_type <= V2G_PAYMENT_SERVICE_SELECTION_MSG or request_type == V2G_SESSION_STOP_MSG)
        return Iso2Test::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);

    received_extra_request = true;
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_payment_service_selection(v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.PaymentServiceSelectionRes;

    report_iso2_request(conn, conn->ctx->current_v2g_msg);
    received_payment_selection = true;

    // Handle the request normally to make sure the request is valid
    const auto next_event = Iso2Test::handle_iso_payment_service_selection(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Override original response code
        res->ResponseCode = iso2_responseCodeType_FAILED;

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

        // Notify board support to turn off the PWM oscillator
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_off();
        report_bsp_event(conn, {types::board_support_common::Event::PowerOff});

        payment_selection_res_time = std::chrono::system_clock::now();
        report_iso2_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(payment_selection_res_time),
            }
        });
    }
    else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling PaymentServiceSelectionReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
            report_iso2_response(conn, conn->ctx->current_v2g_msg, {
                .response_code = res->ResponseCode,
            });
        }

        conn->ctx->test_data.errors.emplace_back("Problems handling PaymentServiceSelectionReq");
        payment_selection_failed = true;
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
    if (received_payment_selection and not payment_selection_failed) {
        // The current time is important for determining connection termination timeouts
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

const char* Iso2TestServer::get_payment_selection_msg_name() {
    return "PaymentServiceSelection";
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_service_detail_payment_selection_003
