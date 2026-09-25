// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_authorization_009.hpp"
#include "tools.hpp"
#include "log.hpp"

using namespace types::evse_test_common;

namespace testing::chn_authorization_009 {

#pragma region COMMON

inline bool is_cp_state_b(const types::board_support_common::Event& event) {
    return event == types::board_support_common::Event::B;
}

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

    const auto now = getmonotonictime();
    const auto request_name = get_authorization_req_name();
    const auto test_data = &(conn->ctx->test_data);

    const auto ongoing_timer_start = auth_start_time;
    const auto ongoing_timer_elapsed = last_auth_req_time - ongoing_timer_start;
    const auto ongoing_timer_expired = ongoing_timer_start + V2G_EVCC_ONGOING_TIMEOUT_60S;

    const auto tcp_close_start = (session_stop_res_time.time_since_epoch().count() > 0)
        ? duration_cast<milliseconds>(session_stop_res_time.time_since_epoch()).count()
        : last_auth_res_time;
    const auto tcp_close_elapsed = now - tcp_close_start;

    if (not received_authorization) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back(fmt::format("EV never initiated {}", request_name));
        return;
    }

    if (authorization_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back(fmt::format("Problems occurred during {}", request_name));
        return;
    }

    if (received_extra_request or authorization_aborted) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate communication");
        return;
    }

    if (auth_start_time == 0LL or last_auth_req_time == 0LL or last_auth_res_time == 0LL) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back(fmt::format("Failed to send {}Res", request_name));
        dlog(DLOG_LEVEL_ERROR, "testing::chn_authorization_009::DinTestServer auth times are unset");
        return;
    }

    /* =======================================================================================
     * Test Validation
     *
     * 1. Validate the time the last AuthorizationReq message arrived to ensure it was sent
     *    within the V2G_EVCC_Ongoing_Timeout but not too early such that a followup request
     *    could have been sent without ambiguity.
     * 2. Validate the control pilot state 'B' started before the timeout period measured from
     *    the time the last AuthorizationRes message was sent.
     * 3. Validate the TCP connection close time to be within the connection termination
     *    timeout period of when the last AuthorizationRes message was sent.
     */

    // Check if the last ContractAuthenticationReq was sent too late (after V2G_EVCC_Ongoing_Timer expired)
    if (last_auth_req_time > ongoing_timer_expired) {
        dlog(DLOG_LEVEL_INFO, "The last ContractAuthenticationReq was sent %d ms after the "
                              "V2G_EVCC_Ongoing_Timer expired.",
                              last_auth_req_time - ongoing_timer_expired);
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate authorization after V2G_EVCC_Ongoing_Timeout expired");
        return;
    }

    // Check if the last ContractAuthenticationReq was sent way too early (before V2G_EVCC_Ongoing_Timer expired)
    if (ongoing_timer_elapsed + sequence_time < V2G_EVCC_ONGOING_TIMEOUT_60S) {
        dlog(DLOG_LEVEL_INFO, "The last ContractAuthenticationReq was sent %d ms before the "
                              "V2G_EVCC_Ongoing_Timer expired.",
                              ongoing_timer_expired - last_auth_req_time);
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV terminated authorization too early");
        return;
    }

    // Whether we reported the connection closure (to avoid double reporting it)
    bool conn_close_reported = false;

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'AuthorizationRes' was sent. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {
        const auto cp_state_b_epoch = timepoint_to_ms(cp_state_b_time);

        // Sometimes the connection is closed before the EV signals CP State 'B'. Let's wait until the end
        // of the 'par_EVCC_StateB_Shutdown_Timeout' duration for the CP State 'B' to be received.
        if (cp_state_b_epoch < last_auth_res_time) {
            // The timer starts shortly after the AuthorizationRes should have been sent (not SessionStopRes)
            const auto cp_state_b_timer_duration = milliseconds(timepoint_to_ms(event.timestamp) - last_auth_res_time);
            const auto cp_state_b_timer_max_duration = milliseconds(V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT);

            // Only wait if we are still within the allowed CP State 'B' duration
            if (cp_state_b_timer_duration < cp_state_b_timer_max_duration) {
                const auto remaining_time = cp_state_b_timer_max_duration - cp_state_b_timer_duration;
                if (wait_for_cp_state_b(remaining_time)) {
                    // Report the connection closure now so that the control pilot transition
                    // is reported afterwards to visually preserve event order.
                    report_connection_closed(conn, {
                        .timestamp = timepoint_to_iso8601_str(event.timestamp),
                        .duration = static_cast<int>(tcp_close_elapsed),
                        .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
                    });
                    conn_close_reported = true;
                }
            }
        }

        // If the `cp_state_b_time` references a point in time earlier than `authorization_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_epoch < last_auth_res_time) {
            // FAIL => EV did not transition to control pilot state B
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("EV never signaled control pilot state B");
        }
        // Since the `cp_state_b_time` references a point in time later than `authorization_res_time`,
        // then the control pilot signal did transition to state 'B' and we must validate that it occurred
        // within an appropriate amount of time.
        else {
            const auto cp_state_b_milliseconds = cp_state_b_epoch - last_auth_res_time;

            constexpr types::board_support_common::BspEvent bsp_event{
                .event = types::board_support_common::Event::B,
            };

            report_bsp_event(conn, bsp_event, {
                .timestamp = timepoint_to_iso8601_str(cp_state_b_time),
                .duration = static_cast<int>(cp_state_b_milliseconds),
                .max_duration = V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT,
            });

            // The EV is required to transition to control pilot state 'B' within
            // 'par_EVCC_StateB_Shutdown_Timeout' of the 'AuthorizationRes' response.
            if (cp_state_b_milliseconds > V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT) {
                // FAIL => EV took too long to transition to control pilot state B
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV took too long to signal control pilot state B");
            }
        }
    }

    // Was a SessionStopReq message received?
    if (session_stop_req_time.time_since_epoch().count() > 0) {

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - monotonic_time_to_system_time(last_auth_res_time));
        const auto session_stop_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        // Was the SessionStopReq sent before Control Pilot transitioned to state 'B'?
        if (validate_control_pilot and monotonic_time_to_system_time(last_auth_res_time) < cp_state_b_time) {
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

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the last 'AuthorizationRes' response.

    if (!conn_close_reported) {
        report_connection_closed(conn, {
            .timestamp = timepoint_to_iso8601_str(event.timestamp),
            .duration = static_cast<int>(tcp_close_elapsed),
            .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
        });
    }

    if (tcp_close_elapsed > V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT) {
        dlog(DLOG_LEVEL_INFO, "EV exceeded TCP close time by %d ms",
             tcp_close_elapsed - V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
    } else if (test_data->outcome == TestOutcome::PreconditionsNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }
}

bool TestBase::is_auth_running_forever(const int64_t time) const {
    const auto ongoing_timer_started = auth_start_time;
    const auto ongoing_timer_elapsed = time - ongoing_timer_started;
    return ongoing_timer_elapsed > V2G_EVCC_ONGOING_TIMEOUT_60S + V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT;
}

#pragma endregion COMMON

#pragma region DIN_70121

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through ContractAuthentication
    if (request_type <= V2G_AUTHORIZATION_MSG or request_type == V2G_SESSION_STOP_MSG) {
        return DinTest::handle_request(conn);
    }

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_contract_authentication(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.ContractAuthenticationRes;
    auto next_event = V2G_EVENT_NO_EVENT;

    // Update timers
    last_auth_req_time = getmonotonictime();
    if (conn->ctx->last_v2g_msg != V2G_AUTHORIZATION_MSG) {
        auth_start_time = last_auth_req_time; // [V2G-DC-CharIN-123]
    }

    // Update sequence timer
    if (last_auth_res_time > 0) {
        const auto last_sequence_time = last_auth_req_time - last_auth_res_time;
        if (last_sequence_time > sequence_time)
            sequence_time = last_sequence_time;
    }

    if (not received_authorization)
        received_authorization = true;

    report_din_request(conn, V2G_AUTHORIZATION_MSG);

    // Fill the EVSE response message
    res->ResponseCode = din_responseCodeType_OK; // [V2G-DC-388]
    res->EVSEProcessing = din_EVSEProcessingType_Ongoing;

    // Check the current response code and check if no external error has occurred
    next_event = din_validate_response_code(&res->ResponseCode, conn);

    // Set next expected req msg
    conn->ctx->state = WAIT_FOR_AUTHORIZATION; // [V2G-DC-444]

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {
        // If ContractAuthentication has been running longer than V2G_EVCC_Ongoing_Timeout and
        // par_CMN_TCP_Connection_Termination_Timeout, then the EV has failed the test.
        if (is_auth_running_forever(last_auth_req_time)) {
            dlog(DLOG_LEVEL_WARNING, "Waiting for authorization forever! Aborting test case");
            res->ResponseCode = din_responseCodeType_FAILED;
            next_event = V2G_EVENT_SEND_AND_TERMINATE;
            authorization_aborted = true;
        }
    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling ContractAuthenticationReq");
        conn->ctx->test_data.errors.emplace_back("Problems handling ContractAuthenticationReq");
        authorization_failed = true;
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        last_auth_res_time = getmonotonictime();

        report_din_response(conn, V2G_AUTHORIZATION_MSG, {
            .response_code = res->ResponseCode,
            .message_fields = {{"EVSEProcessing", "Ongoing"}},
        });

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

    if (received_authorization) {
        // The current time is important for determining SessionSetup message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - monotonic_time_to_system_time(last_auth_res_time));
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

const char* DinTestServer::get_authorization_req_name() {
    return "ContractAuthentication";
}

#pragma endregion DIN_70121

} // namespace testing::chn_authorization_009
