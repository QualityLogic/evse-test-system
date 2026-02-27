// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_welding_session_stop_001 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_DC_VTB_WeldingDetectionOrSessionStop_001</c> implementations defining common
 * fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure and sends a PowerDeliveryRes message with the current SessionID, ResponseCode
 * 'OK', EVSENotification 'StopCharging' and all additional mandatory parameters.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT applies CP State B, sends a correct SessionStopReq message with the current
 * SessionID and all additional mandatory parameters. Furthermore, Test System checks that the SUT terminates the V2G
 * communication session by closing the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout' if a
 * SessionStopRes message with the current SessionID, ResponseCode 'OK' and all additional mandatory parameters was
 * received before. Afterward, the termination of the data link connection within 'par_TP_match_leave' will be checked.
 * If a valid WeldingDetectionReq message was received before, the corresponding message sequence shall be executed by
 * SUT and Test System previously.
 */
class TestBase : public virtual Test {
public:
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// The steady-clock time the first CurrentDemandReq was received.
    long long charging_start_time = 0LL;
    /// The system-clock time the last PowerDeliveryRes message was sent.
    std::chrono::system_clock::time_point power_delivery_res_time;
    /// The system-clock time the last SessionStopRes message was sent.
    std::chrono::system_clock::time_point session_stop_res_time;
    /// The system-clock time the control pilot state changed to 'B'.
    std::chrono::system_clock::time_point cp_state_b_time;

    /// Whether the EVSE is attempting to gracefully stop the EV from charging.
    bool gracefully_stop_charging = false;
    /// Whether the EVSE is attempting to forcefully stop the EV from charging.
    bool forcefully_stop_charging = false;
    /// Whether to validate the control pilot transitions to state B.
    bool validate_control_pilot_b = false;
    /// Whether a response was sent with a "FAILED" response code.
    bool failed_response_message = false;

    /// The maximum amount of time to allow the EV to charge before proceeding with the test.
    static constexpr auto MAXIMUM_CHARGING_DURATION = std::chrono::seconds(30);
    /// The maximum amount of time to allow the EV to gracefully stop charging before getting forceful.
    static constexpr auto MAXIMUM_STOP_CHARGING_DURATION = std::chrono::seconds(10);

    void update_charging_timers(const v2g_connection* conn);
    void require_cp_state_b(const v2g_connection* conn);
};

// [Procedure]
// 1. Normal communication through charging.
//    (How long should charging take place?)
// 2. PowerDeliveryRes(EVSENotification="StopCharging")

// [Validation]
// - Validate ControlPilot state B within timeout.
// - If WeldingDetection is requested, allow it to take place.
// - Validate valid SessionStopReq is received
// - Validate connection closed after SessionStopRes within timeout.
// - (Validate data link connection termination?)
//   (Is this SLAC, and can we detect this?)

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_DC_VTB_WeldingDetectionOrSessionStop_001</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
protected:
    v2g_event din_validate_response_code(din_responseCodeType* din_response_code, const v2g_connection* conn) override;
    v2g_event handle_din_current_demand(v2g_connection* conn) override;
    v2g_event handle_din_power_delivery(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_DC_VTB_WeldingDetectionOrSessionStop_001</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
protected:
    v2g_event iso_validate_response_code(iso2_responseCodeType* v2g_response_code, const v2g_connection* conn) override;
    v2g_event handle_iso_charging_status(v2g_connection* conn) override;
    v2g_event handle_iso_current_demand(v2g_connection* conn) override;
    v2g_event handle_iso_power_delivery(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_welding_session_stop_001
