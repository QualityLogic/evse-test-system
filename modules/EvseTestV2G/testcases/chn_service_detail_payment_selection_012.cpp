// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_service_detail_payment_selection_012.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_service_detail_payment_selection_012 {

#pragma region COMMON

void TestBase::on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {
    if (event.bsp_event == types::board_support_common::Event::F and not set_cp_state_f_time) {
        {
            // =================================================================
            // Do not attempt to acquire the `conn->ctx->test_data.test_mutex`
            // lock before acquiring the `bsp_mutex` lock.
            //
            //     std::lock_guard lock_1(conn->ctx->test_data.test_mutex);
            //     std::lock_guard lock_2(bsp_mutex); // deadlock
            //
            // This could deadlock with `TestBase::signal_cp_state_f()`. If you
            // must acquire the shared text_mutex, acquire the bsp_mutex first.
            // =================================================================

            std::lock_guard lock(bsp_mutex);
            cp_state_f_time = event.timestamp;
            set_cp_state_f_time = true;
        }
        bsp_cv.notify_one();
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

    // If the `cp_state_f_time` time since epoch is '0' then the value was never set.
    if (!detected_cp_state_f || cp_state_f_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to signal control pilot state F");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_service_detail_payment_selection_012::TestBase cp_state_f_time is unset");
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate connection");
        return;
    }

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the control pilot state 'F' signal.

    const auto tcp_close_duration = duration_cast<milliseconds>(event.timestamp - cp_state_f_time);
    const auto tcp_close_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

    report_connection_closed(conn, {
        .timestamp = timepoint_to_iso8601_str(event.timestamp),
        .duration = static_cast<int>(tcp_close_duration.count()),
        .max_duration = static_cast<int>(tcp_close_max_duration.count()),
    });

    if (tcp_close_max_duration < tcp_close_duration) {
        // The EV waited too long to terminate the connection
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate the connection before the "
                                       "'par_CMN_TCP_Connection_Termination_Timeout' timer expired");
    }
    else if (test_data->outcome != TestOutcome::PassCriteriaNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }
}

template <typename _Rep, typename _Period>
bool TestBase::wait_for_cp_state_f(const std::chrono::duration<_Rep, _Period>& timeout) {
    std::unique_lock lock(bsp_mutex);
    return bsp_cv.wait_for(lock, timeout, [this] { return set_cp_state_f_time; });
}

template <typename _Rep, typename _Period>
bool TestBase::signal_cp_state_f(v2g_connection* conn, const std::chrono::duration<_Rep, _Period>& timeout) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    // Handle if a BSP state 'F' event has already occurred
    {
        std::lock_guard lock_1(bsp_mutex);

        // Has a control pilot state 'F' already been detected?
        if (set_cp_state_f_time == true) {

            types::board_support_common::Event current_state;

            {
                // This would only deadlock if on_update_bsp_event attempts
                // to acquire these locks in reverse order.
                std::lock_guard lock_2(conn->ctx->test_data.test_mutex);
                current_state = conn->ctx->test_data.cp_state;
            }

            // Is the current control pilot state 'F'?
            if (current_state == types::board_support_common::Event::F) {
                dlog(DLOG_LEVEL_ERROR, "Control pilot state 'F' was already detected and is still active.");
                // A control pilot state 'F' was already detected and is still active.
                // State 'F' indicates a charger fault, and we should not continue.
                return false;
            }

            const auto state_name = types::board_support_common::event_to_string(current_state);
            dlog(DLOG_LEVEL_WARNING, "Control pilot state 'F' was already detected but changed to state %s. "
                                     "Resetting state 'F' detection.", state_name.c_str());

            // For some reason, a control pilot state 'F' was detected while
            // this test was running, but was somehow cleared. Since Everest
            // is allowing the session to continue, we'll reset the state F
            // detection and proceed with the test.
            set_cp_state_f_time = false;
        }
    }

    // Suppress the next control pilot state 'F' event from automatically closing the
    // connection so that we can see how long it takes for the EV to close the connection.
    conn->ctx->force_next_connection_req_res = true;

    dlog(DLOG_LEVEL_INFO, "Suppressing next control pilot state 'F' event from closing the connection.");

    // Ask the board support module to signal control pilot state 'F'
    conn->ctx->r_bsp->call_pwm_F();

    // Wait for the control pilot state 'F' to be detected
    if (wait_for_cp_state_f(timeout)) {
        // Control pilot state 'F' was detected within timeout
        detected_cp_state_f = true;

        // Report that control pilot state 'F' was detected
        report_bsp_event(conn, {types::board_support_common::Event::F}, {
            .timestamp = timepoint_to_iso8601_str(cp_state_f_time),
        });

        // Pause for 0.5 seconds to give the EV a chance to detect the control pilot state 'F'
        const auto sleep_duration = cp_state_f_time + milliseconds(500) - system_clock::now();
        const auto sleep_milliseconds = duration_cast<milliseconds>(sleep_duration).count();

        if (sleep_milliseconds > 0) {
            dlog(DLOG_LEVEL_INFO, "Pausing for %d milliseconds to allow CP state F to be detected by EV",
                 sleep_milliseconds);

            std::this_thread::sleep_until(cp_state_f_time + milliseconds(500));
        }

    } else {
        // Timeout occurred
        detected_cp_state_f = false;

        dlog(DLOG_LEVEL_ERROR, "Timed out waiting for control pilot state 'F' confirmation");
    }

    return detected_cp_state_f;
}

#pragma endregion COMMON


#pragma region DIN_70121

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through ServicePaymentSelection
    // if (request_type <= V2G_PAYMENT_SERVICE_SELECTION_MSG or request_type == V2G_SESSION_STOP_MSG)
    //     return DinTest::handle_request(conn);

    if (request_type < V2G_POWER_DELIVERY_MSG or request_type == V2G_SESSION_STOP_MSG) {
        if (cp_state_f_time.time_since_epoch().count() > 0) {
            const auto now = std::chrono::system_clock::now();
            const auto elapsed = cp_state_f_time - now;
            if (elapsed <= std::chrono::milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
                return DinTest::handle_request(conn);
            }
        } else {
            return DinTest::handle_request(conn);
        }
    }

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);

    received_extra_request = true;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_service_payment_selection(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.ServicePaymentSelectionRes;

    report_din_request(conn, conn->ctx->current_v2g_msg);
    received_payment_selection = true;

    // Handle the request normally to make sure the request is valid
    auto next_event = DinTest::handle_din_service_payment_selection(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Signal control pilot state 'F' and wait for confirmation
        if (signal_cp_state_f(conn, std::chrono::seconds(1))) {
            // Handle if an EVSE shutdown was requested while we were waiting
            if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
                if (conn->ctx->terminate_connection_on_failed_response)
                    next_event = V2G_EVENT_SEND_AND_TERMINATE;
                res->ResponseCode = din_responseCodeType_FAILED;
                payment_selection_failed = true;
            }
        } else {
            // Handle if a timeout occurred waiting for confirmation
            next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = din_responseCodeType_FAILED;
        }
    }
    else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling ServicePaymentSelectionReq");
        conn->ctx->test_data.errors.emplace_back("Problems handling ServicePaymentSelectionReq");
        payment_selection_failed = true;
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_din_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    if (detected_cp_state_f) {
        // The current time is important for determining SessionSetup message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - cp_state_f_time);
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

    // Allow the communication to continue normally through PaymentServiceSelection
    // if (request_type <= V2G_PAYMENT_SERVICE_SELECTION_MSG)
    //     return Iso2Test::handle_request(conn);

    if (request_type < V2G_POWER_DELIVERY_MSG or request_type == V2G_SESSION_STOP_MSG) {
        if (cp_state_f_time.time_since_epoch().count() > 0) {
            const auto now = std::chrono::system_clock::now();
            const auto elapsed = cp_state_f_time - now;
            if (elapsed <= std::chrono::milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
                return Iso2Test::handle_request(conn);
            }
        } else {
            return Iso2Test::handle_request(conn);
        }
    }

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);

    received_extra_request = true;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");
    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_payment_service_selection(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.PaymentServiceSelectionRes;

    report_iso2_request(conn, conn->ctx->current_v2g_msg);
    received_payment_selection = true;

    // Handle the request normally to make sure the request is valid
    auto next_event = Iso2Test::handle_iso_payment_service_selection(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Signal control pilot state 'F' and wait for confirmation
        if (signal_cp_state_f(conn, std::chrono::seconds(1))) {
            // Handle if an EVSE shutdown was requested while we were waiting
            if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
                if (conn->ctx->terminate_connection_on_failed_response)
                    next_event = V2G_EVENT_SEND_AND_TERMINATE;
                res->ResponseCode = iso2_responseCodeType_FAILED;
                payment_selection_failed = true;
            }
        } else {
            // Handle if a timeout occurred waiting for confirmation
            next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = iso2_responseCodeType_FAILED;
        }
    }
    else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling PaymentServiceSelectionReq");
        conn->ctx->test_data.errors.emplace_back("Problems handling PaymentServiceSelectionReq");
        payment_selection_failed = true;
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_iso2_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;
    v2g_event next_event;

    // Event reporting is only relevant if the test was actually carried out (current demand was reached)
    if (detected_cp_state_f) {
        // The current time is important for determining SessionSetup message timeouts
        session_stop_req_time = system_clock::now();

        const auto session_stop_duration = duration_cast<milliseconds>(session_stop_req_time - cp_state_f_time);
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

} // namespace testing::chn_service_detail_payment_selection_012
