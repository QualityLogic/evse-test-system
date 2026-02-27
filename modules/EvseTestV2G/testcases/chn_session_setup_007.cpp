// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_session_setup_007.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_session_setup_007 {

//=============================================
//             Event Handling
//=============================================

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto elapsed = duration_cast<milliseconds>(event.timestamp - cp_state_f_time).count();
    const auto test_data = &(conn->ctx->test_data);

    if (not received_session_setup) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated SessionSetup");
        return;
    }

    if (session_setup_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during SessionSetup");
        return;
    }

    if (received_extra_request) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate communication");
        return;
    }

    // If the `cp_state_f_time` time since epoch is '0' then the value was never set.
    if (cp_state_f_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to signal control pilot state F");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_session_setup_007::TestBase cp_state_f_time is unset");
        return;
    }

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the control pilot state 'F' signal.

    report_connection_closed(conn, {
        .timestamp = timepoint_to_iso8601_str(event.timestamp),
        .duration = static_cast<int>(elapsed),
        .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
    });

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

    // Allow the communication to continue normally through SessionSetup
    // if (request_type <= V2G_SESSION_SETUP_MSG)
    //     return DinTest::handle_request(conn);

    if (request_type < V2G_POWER_DELIVERY_MSG) {
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

v2g_event DinTestServer::handle_din_session_setup(v2g_connection* conn) {

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionSetupRes;

    report_din_request(conn, V2G_SESSION_SETUP_MSG);
    received_session_setup = true;

    // Handle the request normally to make sure the request was valid
    auto next_event = DinTest::handle_din_session_setup(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Notify board support to signal control pilot state 'F'
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_F();

        // Record the current time the control pilot state changed
        cp_state_f_time = std::chrono::system_clock::now();
        report_bsp_event(conn, {types::board_support_common::Event::F}, {
            .timestamp = timepoint_to_iso8601_str(cp_state_f_time),
        });

        // Pause for exactly 0.5 seconds before sending the response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Handle if an EVSE shutdown was requested while we were waiting
        if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
            if (conn->ctx->terminate_connection_on_failed_response)
                next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = din_responseCodeType_FAILED;
            session_setup_failed = true;
        }

        report_din_response(conn, V2G_SESSION_SETUP_MSG, {res->ResponseCode});
    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling SessionSetupReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE)
            report_din_response(conn, V2G_SESSION_SETUP_MSG, {res->ResponseCode});

        conn->ctx->test_data.errors.emplace_back("Problems handling SessionSetupReq");
        session_setup_failed = true;
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

    // Allow the communication to continue normally through SessionSetup
    // if (request_type <= V2G_SESSION_SETUP_MSG)

    if (request_type < V2G_POWER_DELIVERY_MSG) {
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

v2g_event Iso2TestServer::handle_iso_session_setup(v2g_connection* conn) {

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionSetupRes;

    report_iso2_request(conn, V2G_SESSION_SETUP_MSG);
    received_session_setup = true;

    // Handle the request normally to make sure the request was valid
    auto next_event = Iso2Test::handle_iso_session_setup(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Notify board support to signal control pilot state 'F'
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_F();

        // Record the current time the control pilot state changed
        cp_state_f_time = std::chrono::system_clock::now();
        report_bsp_event(conn, {types::board_support_common::Event::F}, {
            .timestamp = timepoint_to_iso8601_str(cp_state_f_time),
        });

        // Pause for exactly 0.5 seconds before sending the response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Handle if an EVSE shutdown was requested while we were waiting
        if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
            if (conn->ctx->terminate_connection_on_failed_response)
                next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = iso2_responseCodeType_FAILED;
            session_setup_failed = true;
        }

        report_iso2_response(conn, V2G_SESSION_SETUP_MSG, {res->ResponseCode});

    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling SessionSetupReq");

        if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE)
            report_iso2_response(conn, V2G_SESSION_SETUP_MSG, {res->ResponseCode});

        conn->ctx->test_data.errors.emplace_back("Problems handling SessionSetupReq");
        session_setup_failed = true;
    }

    return next_event;
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_session_setup_007
