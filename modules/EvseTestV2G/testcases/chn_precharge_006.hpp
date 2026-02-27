// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef CHN_PRECHARGE_006_HPP
#define CHN_PRECHARGE_006_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_precharge_006 {

// Yes, the CharIN version of 'TC_EVCC_DC_VTB_PreCharge_006' is called 'CharIN_TC_EVCC_DC_VTB_PreCharge_007'
/**
 * @class TestBase
 * @brief A base class for all <c>CharIN_TC_EVCC_DC_VTB_PreCharge_007</c> implementations defining common fields.
 */
class TestBase : public virtual Test {
public:
    // === Event Handling ===
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// Whether the PreCharge stage of communication was reached
    bool precharge_started = false;
    /// Whether problems occurred during PreCharge
    bool precharge_failed = false;
    /// Whether PreCharge was stopped due to timers expiring
    bool precharge_aborted = false;
    /// Whether an unexpected request was received after PreCharge
    bool received_extra_request = false;
    /// Whether the control pilot state was not 'B' at the time of deviation and a transition is required.
    bool validate_control_pilot = false;
    /// The target voltage requested by the EV during PreCharge
    float target_voltage = 0;
    /// The maximum present voltage measured during PreCharge
    float max_present_voltage = 0;

    /// The system time the last control pilot state 'B' transition occurred.
    std::chrono::system_clock::time_point cp_state_b_time;
    /// The system time the first PreChargeRes was received.
    std::chrono::system_clock::time_point first_precharge_req_time;
    /// The system time the last PreChargeRes was received.
    std::chrono::system_clock::time_point last_precharge_req_time;
    /// The system time the last PreChargeRes was sent.
    std::chrono::system_clock::time_point last_precharge_res_time;
    /// The system time the last SessionStopRes was sent.
    std::chrono::system_clock::time_point last_session_stop_time;

    /// Maximum number of milliseconds between a PreChargeRes and a followup PreChargeReq
    std::chrono::milliseconds sequence_time{0};

    /**
     * @brief Check if a time has unambiguously exceeded the precharge timeout.
     * @param time The monotonic time to compare against the precharge timers.
     * @return <c>true</c> if the timers have unambiguously expired, otherwise <c>false</c>.
     */
    bool is_precharge_running_forever(const std::chrono::system_clock::time_point& time) const;
};

/**
 * @class DinTestServer
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_PreCharge_007</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    // === Request Publishing ===
    void publish_din_precharge_req(v2g_context* ctx, const din_PreChargeReqType* v2g_precharge_req) override;

    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_pre_charge(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_PreCharge_007</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    // === Request Publishing ===
    void publish_iso_pre_charge_req(v2g_context* ctx, const iso2_PreChargeReqType* v2g_precharge_req) override;

    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_pre_charge(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_precharge_006

#endif //CHN_PRECHARGE_006_HPP
