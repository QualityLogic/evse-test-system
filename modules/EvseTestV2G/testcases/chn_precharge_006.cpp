// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chn_precharge_006.hpp"
#include "report_tools.hpp"
#include "tools.hpp"
#include "v2g_ctx.hpp"
#include "log.hpp"

using namespace types::evse_test_common;

namespace testing::chn_precharge_006 {

bool TestBase::is_precharge_running_forever(const std::chrono::system_clock::time_point& time) const {
    using std::chrono::milliseconds;
    const auto precharge_timer_started = first_precharge_req_time;
    const auto precharge_timer_elapsed = time - precharge_timer_started;
    return precharge_timer_elapsed > milliseconds(V2G_EVCC_PRECHARGE_TIMEOUT + V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);
}

inline bool is_cp_state_b(const types::board_support_common::Event& event) {
    return event == types::board_support_common::Event::B;
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

    const auto test_data = &(conn->ctx->test_data);

    const auto precharge_timer_start = first_precharge_req_time;
    const auto precharge_timer_elapsed = duration_cast<milliseconds>(last_precharge_req_time - precharge_timer_start);
    const auto precharge_timer_expired = precharge_timer_start + milliseconds(V2G_EVCC_PRECHARGE_TIMEOUT);

    // TCP closure begins either after the last PreChargeRes or SessionStopRes (whichever occurred most recently)
    const auto tcp_close_start = last_session_stop_time > last_precharge_res_time ? last_session_stop_time : last_precharge_res_time;
    const auto tcp_close_elapsed = duration_cast<milliseconds>(event.timestamp - tcp_close_start);

    if (not precharge_started) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never initiated PreCharge");
        return;
    }

    if (precharge_failed) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Problems occurred during PreCharge");
        return;
    }

    if (received_extra_request or precharge_aborted) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV did not terminate communication");
        return;
    }

    // If the time since epoch is '0' then the value was never set.
    if ((first_precharge_req_time.time_since_epoch().count() == 0) or
        (last_precharge_req_time.time_since_epoch().count() == 0) or
        (last_precharge_res_time.time_since_epoch().count() == 0)) {

        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("Failed to send PreChargeRes");
        dlog(DLOG_LEVEL_ERROR, "testing::chn_precharge_006 timers unset!");
        return;
    }

    // Check if the last PreChargeReq was sent too late (after V2G_EVCC_PreCharge_Timer expired)
    if (last_precharge_req_time > precharge_timer_expired) {
        dlog(DLOG_LEVEL_INFO, "Last PreChargeReq was sent %d ms after the V2G_EVCC_PreCharge_Timer expired.",
             duration_cast<milliseconds>(last_precharge_req_time - precharge_timer_expired).count());

        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV sent PreChargeReq after V2G_EVCC_PreCharge_Timeout expired");
        return;
    }

    // Check if the last PreChargeReq was sent way too early (before V2G_EVCC_PreCharge_Timer expired)
    if (precharge_timer_elapsed + sequence_time < milliseconds(V2G_EVCC_PRECHARGE_TIMEOUT)) {
        dlog(DLOG_LEVEL_INFO, "Last PreChargeReq was sent %d ms before the V2G_EVCC_PreCharge_Timer expired.",
             duration_cast<milliseconds>(precharge_timer_expired - last_precharge_req_time).count());

        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV terminated authorization too early");
        return;
    }

    // Check if the last SessionStopReq was sent way too late (after 'par_CMN_TCP_Connection_Termination_Timeout')
    if (last_session_stop_time - last_precharge_res_time > milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
        dlog(DLOG_LEVEL_INFO, "Last SessionStopReq was sent %d ms after the last PreChargeRes",
             duration_cast<milliseconds>(last_session_stop_time - last_precharge_res_time).count());

        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to send a SessionStopReq");
    }

    // If `validate_control_pilot` is 'true' then the control pilot was not in state 'B' at the time
    // the 'V2G_EVCC_PreCharge_Timer' expired. We must validate that the EV transitioned to state 'B'.
    if (validate_control_pilot) {

        // If the `cp_state_b_time` references a point in time earlier than `last_precharge_res_time`,
        // then the control pilot signal never transitioned to state 'B' before the connection closed.
        if (cp_state_b_time < last_precharge_res_time) {
            // FAIL => EV did not transition to control pilot state B
            test_data->outcome = TestOutcome::PassCriteriaNotMet;
            test_data->errors.emplace_back("== EV did not signal control pilot state B ==");
        }
        // Since the `cp_state_b_time` references a point in time later than `last_precharge_res_time`,
        // then the control pilot signal did transition to state 'B' and we must validate that it occurred
        // within an appropriate amount of time.
        else {
            const auto cp_state_b_duration = cp_state_b_time - last_precharge_res_time;
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
            if (cp_state_b_milliseconds > V2G_TEST_EVCC_STATE_B_SHUTDOWN_TIMEOUT) {
                // FAIL => EV took too long to transition to control pilot state B
                test_data->outcome = TestOutcome::PassCriteriaNotMet;
                test_data->errors.emplace_back("EV took too long to signal control pilot state B");
            }
        }
    }

    // The EV is required to terminate the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'
    // of the last 'PreChargeRes' or 'SessionStopRes' response.

    report_connection_closed(conn, {
        .timestamp = timepoint_to_iso8601_str(event.timestamp),
        .duration = static_cast<int>(tcp_close_elapsed.count()),
        .max_duration = V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT,
    });

    if (tcp_close_elapsed > milliseconds(V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT)) {
        dlog(DLOG_LEVEL_INFO, "EV exceeded TCP close time by %d ms",
             tcp_close_elapsed.count() - V2G_TEST_TCP_CONNECTION_TERMINATION_TIMEOUT);

        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV took too long to terminate the TCP connection");
    } else if (test_data->outcome == TestOutcome::PreconditionsNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }
}

#pragma region DIN_70121

//=============================================
//             Request Publishing
//=============================================

void DinTestServer::publish_din_precharge_req(v2g_context* ctx, const din_PreChargeReqType* v2g_precharge_req) {
    // This test case requires the EVSE to maintain a present voltage of 0V during the entire PreCharge phase.
    // To achieve this, we will publish a target voltage of 0V.
    constexpr auto target_voltage = 0.0;
    const auto target_current = calc_physical_value(
        v2g_precharge_req->EVTargetCurrent.Value, v2g_precharge_req->EVTargetCurrent.Multiplier);
    publish_dc_ev_target_voltage_current(ctx, target_voltage, static_cast<float>(target_current));
    publish_DIN_DcEvStatus(ctx, v2g_precharge_req->DC_EVStatus);
}

//=============================================
//             Request Handling
//=============================================

v2g_event DinTestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through PreCharge and allow SessionStop
    if (request_type <= V2G_PRE_CHARGE_MSG or request_type == V2G_SESSION_STOP_MSG)
        return DinTest::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_din_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event DinTestServer::handle_din_pre_charge(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.PreChargeReq;
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.PreChargeRes;
    const auto now = system_clock::now();

    // Initialize the test if this is the first PreChargeReq received
    if (conn->ctx->last_v2g_msg != V2G_PRE_CHARGE_MSG) {
        precharge_started = true;
        first_precharge_req_time = now;
        target_voltage = static_cast<float>(calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier));
    }

    // Update sequence timer
    if (last_precharge_res_time.time_since_epoch().count() > 0) {
        const auto last_sequence_time =
            duration_cast<milliseconds>(last_precharge_req_time - last_precharge_res_time);
        if (last_sequence_time > sequence_time)
            sequence_time = last_sequence_time;
    }

    last_precharge_req_time = now;

    // Report EV request message
    const auto target_voltage_str = din_PhysicalValueType_to_string(req->EVTargetVoltage, 1);
    const auto target_current_str = din_PhysicalValueType_to_string(req->EVTargetCurrent, 1);
    report_din_request(conn, V2G_PRE_CHARGE_MSG, {
        .message_fields = {
            {"EVTargetVoltage", target_voltage_str.c_str()},
            {"EVTargetCurrent", target_current_str.c_str()},
        },
        .metadata = {
            .timestamp = timepoint_to_iso8601_str(last_precharge_req_time),
        }
    });

    // Handle the PreChargeReq message normally
    auto next_event = DinTest::handle_din_pre_charge(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        // This test case requires providing and communicating 0V of present voltage during PreCharge.
        // EVs are required to measure the present voltage themselves rather than blindly trust the measurement
        // communicated by the PreChargeRes. While we will always list 0V in the response, it is noteworthy
        // if voltage was actually present as this could influence the EV's behavior.

        const auto evse_present_voltage =
            calc_physical_value(res->EVSEPresentVoltage.Value,
                                res->EVSEPresentVoltage.Multiplier);

        if (evse_present_voltage > max_present_voltage) {
            max_present_voltage = static_cast<float>(evse_present_voltage);
        }

        if (evse_present_voltage > 0) {
            dlog(DLOG_LEVEL_WARNING, "EVSE present voltage is %.1fV when 0V is expected", evse_present_voltage);

            res->EVSEPresentVoltage.Value = 0;
            res->EVSEPresentVoltage.Multiplier = 1;
        }

        if (res->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_Ready) {
            dlog(DLOG_LEVEL_WARNING, "PreChargeRes EVSE status code is '%s' when 'EVSE_Ready' is expected",
                din_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode));
        }

        // If PreCharge has been running longer than V2G_EVCC_PreCharge_Timeout and
        // par_CMN_TCP_Connection_Termination_Timeout, then the EV has failed the test.
        if (is_precharge_running_forever(last_precharge_req_time)) {
            dlog(DLOG_LEVEL_WARNING, "Waiting for PreCharge forever! Aborting test case");
            res->ResponseCode = din_responseCodeType_FAILED;
            next_event = V2G_EVENT_SEND_AND_TERMINATE;
            precharge_aborted = true;
        }
    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling PreChargeReq");
        conn->ctx->test_data.errors.emplace_back("Problems handling PreChargeReq");
        precharge_failed = true;
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        const auto present_voltage_str = din_PhysicalValueType_to_string(res->EVSEPresentVoltage, 1);

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            validate_control_pilot = not is_cp_state_b(conn->ctx->test_data.cp_state);
        }

        last_precharge_res_time = system_clock::now();

        report_din_response(conn, V2G_PRE_CHARGE_MSG, {
            .response_code = res->ResponseCode,
            .message_fields = {
                {"EVSEPresentVoltage", present_voltage_str.c_str()},
            },
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(last_precharge_res_time),
            }
        });
    }

    return next_event;
}

v2g_event DinTestServer::handle_din_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination

    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;

    report_din_request(conn, V2G_SESSION_STOP_MSG);

    const auto v2g_event = DinTest::handle_din_session_stop(conn);

    // Take note of the current time to later calculate the TCP connection termination duration
    last_session_stop_time = std::chrono::system_clock::now();

    if (v2g_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_din_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(last_session_stop_time),
            }
        });
    }

    return v2g_event;
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

//=============================================
//             Request Publishing
//=============================================

void Iso2TestServer::publish_iso_pre_charge_req(v2g_context* ctx, const iso2_PreChargeReqType* v2g_precharge_req) {
    // This test case requires the EVSE to maintain a present voltage of 0V during the entire PreCharge phase.
    // To achieve this, we will publish a target voltage of 0V.
    constexpr auto target_voltage = 0.0;
    const auto target_current = calc_physical_value(
        v2g_precharge_req->EVTargetCurrent.Value, v2g_precharge_req->EVTargetCurrent.Multiplier);
    publish_dc_ev_target_voltage_current(ctx, target_voltage, static_cast<float>(target_current));
    publish_DcEvStatus(ctx, v2g_precharge_req->DC_EVStatus);
}

//=============================================
//             Request Handling
//=============================================

v2g_event Iso2TestServer::handle_request(v2g_connection* conn) {
    const auto request_type = find_req_message_type(conn);

    // Allow the communication to continue normally through PreCharge and allow SessionStop
    if (request_type <= V2G_PRE_CHARGE_MSG or request_type == V2G_SESSION_STOP_MSG)
        return Iso2Test::handle_request(conn);

    dlog(DLOG_LEVEL_WARNING, "Received an unexpected request from EV - stopping test");

    report_iso2_request(conn, request_type);
    received_extra_request = true;

    conn->ctx->test_data.outcome = TestOutcome::PassCriteriaNotMet;
    conn->ctx->test_data.errors.emplace_back("EV sent an unexpected request");

    // It is unsafe to continue charging beyond this point
    return V2G_EVENT_TERMINATE_CONNECTION;
}

v2g_event Iso2TestServer::handle_iso_pre_charge(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::system_clock;

    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.PreChargeReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.PreChargeRes;
    const auto now = system_clock::now();

    // Initialize the test if this is the first PreChargeReq received
    if (conn->ctx->last_v2g_msg != V2G_PRE_CHARGE_MSG) {
        precharge_started = true;
        first_precharge_req_time = now;
        target_voltage = static_cast<float>(calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier));
    }

    // Update sequence timer
    if (last_precharge_res_time.time_since_epoch().count() > 0) {
        const auto last_sequence_time =
            duration_cast<milliseconds>(last_precharge_req_time - last_precharge_res_time);
        if (last_sequence_time > sequence_time)
            sequence_time = last_sequence_time;
    }

    last_precharge_req_time = now;

    // Report EV request message
    const auto target_voltage_str = iso2_PhysicalValueType_to_string(req->EVTargetVoltage, 1);
    const auto target_current_str = iso2_PhysicalValueType_to_string(req->EVTargetCurrent, 1);
    report_iso2_request(conn, V2G_PRE_CHARGE_MSG, {
        .message_fields = {
            {"EVTargetVoltage", target_voltage_str.c_str()},
            {"EVTargetCurrent", target_current_str.c_str()},
        },
        .metadata = {
            .timestamp = timepoint_to_iso8601_str(last_precharge_req_time),
        }
    });

    // Handle the PreChargeReq message normally
    auto next_event = Iso2Test::handle_iso_pre_charge(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        // This test case requires providing and communicating 0V of present voltage during PreCharge.
        // EVs are required to measure the present voltage themselves rather than blindly trust the measurement
        // communicated by the PreChargeRes. While we will always list 0V in the response, it is noteworthy
        // if voltage was actually present as this could influence the EV's behavior.

        const auto evse_present_voltage =
            calc_physical_value(res->EVSEPresentVoltage.Value,
                                res->EVSEPresentVoltage.Multiplier);

        if (evse_present_voltage > max_present_voltage) {
            max_present_voltage = static_cast<float>(evse_present_voltage);
        }

        if (evse_present_voltage > 0) {
            dlog(DLOG_LEVEL_WARNING, "EVSE present voltage is %.1fV when 0V is expected", evse_present_voltage);

            populate_physical_value_float(&res->EVSEPresentVoltage, 0, 1, iso2_unitSymbolType_V);
        }

        if (res->DC_EVSEStatus.EVSEStatusCode != iso2_DC_EVSEStatusCodeType_EVSE_Ready) {
            dlog(DLOG_LEVEL_WARNING, "PreChargeRes EVSE status code is '%s' when 'EVSE_Ready' is expected",
                iso2_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode));
        }

        // If PreCharge has been running longer than V2G_EVCC_PreCharge_Timeout and
        // par_CMN_TCP_Connection_Termination_Timeout, then the EV has failed the test.
        if (is_precharge_running_forever(last_precharge_req_time)) {
            dlog(DLOG_LEVEL_WARNING, "Waiting for PreCharge forever! Aborting test case");
            res->ResponseCode = iso2_responseCodeType_FAILED;
            next_event = V2G_EVENT_SEND_AND_TERMINATE;
            precharge_aborted = true;
        }
    } else {
        dlog(DLOG_LEVEL_ERROR, "Problems handling PreChargeReq");
        conn->ctx->test_data.errors.emplace_back("Problems handling PreChargeReq");
        precharge_failed = true;
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        const auto present_voltage_str = iso2_PhysicalValueType_to_string(res->EVSEPresentVoltage, 1);

        // If the current control pilot state is not 'B', we must validate that it transitions
        {
            std::lock_guard lock(conn->ctx->test_data.test_mutex);
            validate_control_pilot = not is_cp_state_b(conn->ctx->test_data.cp_state);
        }

        last_precharge_res_time = system_clock::now();

        report_iso2_response(conn, V2G_PRE_CHARGE_MSG, {
            .response_code = res->ResponseCode,
            .message_fields = {
                {"EVSEPresentVoltage", present_voltage_str.c_str()},
            },
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(last_precharge_res_time),
            }
        });
    }

    return next_event;
}

v2g_event Iso2TestServer::handle_iso_session_stop(v2g_connection* conn) {
    // If a SessionStopReq message with the current SessionID and all additional mandatory parameters
    // was received before, Test System sends a valid SessionStopRes message, restarts the timer
    // 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;

    report_iso2_request(conn, V2G_SESSION_STOP_MSG);

    const auto v2g_event = Iso2Test::handle_iso_session_stop(conn);

    // Take note of the current time to later calculate the TCP connection termination duration
    last_session_stop_time = std::chrono::system_clock::now();

    if (v2g_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_iso2_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
            .metadata = {
                .timestamp = timepoint_to_iso8601_str(last_session_stop_time),
            }
        });
    }

    return v2g_event;
}

#pragma endregion ISO_15118_2

} // namespace testing::chn_precharge_006
