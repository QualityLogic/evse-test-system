// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_current_demand_007.hpp"
#include "log.hpp"
#include "tools.hpp"

using namespace types::evse_test_common;

namespace testing::chn_current_demand_007 {

#pragma region Common

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

void TestBase::on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    // Ensure the EVSE is no longer attempting to charge the EV
    open_contactors(conn);

    const auto test_data = &(conn->ctx->test_data);
    const auto tcp_close_duration = duration_cast<milliseconds>(event.timestamp - cp_state_f_time);
    const auto tcp_close_max_duration = milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

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
        test_data->errors.emplace_back("EV did not terminate the connection");
        return;
    }

    // If the `cp_state_f_time` time since epoch is '0' then the value was never set.
    if (cp_state_f_time.time_since_epoch().count() == 0) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to signal control pilot state F");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_current_demand_007::TestBase cp_state_f_time is unset!");
        return;
    }

    // The EV is required to terminate the connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the control pilot state 'F' signal.

    report_connection_closed(conn, {
        .timestamp = timepoint_to_iso8601_str(event.timestamp),
        .duration = static_cast<int>(tcp_close_duration.count()),
        .max_duration = static_cast<int>(tcp_close_max_duration.count()),
    });

    if (tcp_close_duration <= tcp_close_max_duration) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    } else {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
    }
}

#pragma endregion Common

#pragma region DIN_70121

//=============================================
//             Request Handling
//=============================================

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through the first CurrentDemandReq
    if (request_type <= V2G_CURRENT_DEMAND_MSG and not current_demand_started) {
        return DinTest::handle_request(conn);
    }
    // Allow the communication to continue until the TCP termination timer expires
    if (cp_state_f_time.time_since_epoch().count() > 0) {
        const auto now = std::chrono::system_clock::now();
        const auto elapsed = cp_state_f_time - now;
        if (elapsed <= std::chrono::milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
            return DinTest::handle_request(conn);
        }
    }

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

    current_demand_started = true;
    current_demand_failed = false;
    report_din_request(conn, conn->ctx->current_v2g_msg);

    // Allow the CurrentDemandReq to be handled normally
    auto next_event = DinTest::handle_din_current_demand(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // Notify board support to signal control pilot state 'F'
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_F();

        // Report the current time the control pilot state changed
        cp_state_f_time = std::chrono::system_clock::now();
        report_bsp_event(conn, {types::board_support_common::Event::F}, {
            .timestamp = timepoint_to_iso8601_str(cp_state_f_time),
        });

        // Pause for 0.5 seconds before releasing the response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Handle if an EVSE shutdown was requested while we were waiting
        if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
            if (conn->ctx->terminate_connection_on_failed_response)
                next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = din_responseCodeType_FAILED;
            current_demand_failed = true;
        }

        report_din_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
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

#pragma endregion DIN_70121

#pragma region ISO_15118_2

//=============================================
//             Request Handling
//=============================================

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // This test is only compatible with 'DC' charging. If this is an 'AC' charger,
    // we'll gracefully abort the charging session to avoid fully charging the EV.
    if (conn->ctx->is_dc_charger == false) {
        if (not received_extra_request) {
            dlog(DLOG_LEVEL_WARNING, "DC-only test invoked as AC - stopping test");

            received_extra_request = true;
            conn->ctx->test_data.errors.emplace_back("AC charger does not support DC-only test");
            conn->ctx->stop_hlc = true;
        }
        return Iso2Test::handle_request(conn);
    }

    // Allow the communication to continue through the first CurrentDemandReq and allow SessionStopReq
    if (request_type <= V2G_CURRENT_DEMAND_MSG and not current_demand_started) {

        return Iso2Test::handle_request(conn);
    }

    // Allow the communication to continue until the TCP termination timer expires
    if (cp_state_f_time.time_since_epoch().count() > 0) {
        const auto now = std::chrono::system_clock::now();
        const auto elapsed = cp_state_f_time - now;
        if (elapsed <= std::chrono::milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
            return Iso2Test::handle_request(conn);
        }
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

    current_demand_started = true;
    current_demand_failed = false;
    report_iso2_request(conn, conn->ctx->current_v2g_msg);

    // Allow the CurrentDemandReq to be handled normally
    auto next_event = Iso2Test::handle_iso_current_demand(conn);

    // The test should only be performed if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // Notify board support to signal control pilot state 'F'
        conn->ctx->force_next_connection_req_res = true;
        conn->ctx->r_bsp->call_pwm_F();

        // Report the current time the control pilot state changed
        cp_state_f_time = std::chrono::system_clock::now();
        report_bsp_event(conn, {types::board_support_common::Event::F}, {
            .timestamp = timepoint_to_iso8601_str(cp_state_f_time),
        });

        // Pause for 0.5 seconds before releasing the response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Handle if an EVSE shutdown was requested while we were waiting
        if (conn->ctx->stop_hlc or conn->ctx->intl_emergency_shutdown) {
            if (conn->ctx->terminate_connection_on_failed_response)
                next_event = V2G_EVENT_SEND_AND_TERMINATE;
            res->ResponseCode = iso2_responseCodeType_FAILED;
            current_demand_failed = true;
        }

        report_iso2_response(conn, conn->ctx->current_v2g_msg, {
            .response_code = res->ResponseCode,
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

#pragma endregion ISO_15118_2

} // namespace testing::chn_current_demand_007
