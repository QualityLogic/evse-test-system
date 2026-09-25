// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef EVEREST_TEST_SYSTEM_CHN_CABLE_CHECK_006_HPP
#define EVEREST_TEST_SYSTEM_CHN_CABLE_CHECK_006_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_cable_check_006 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_DC_VTB_CableCheck_006</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, sends continuously a CableCheckRes message with the current SessionID,
 * ResponseCode 'OK', EVSEProcessing 'Ongoing' EVSEStatusCode 'EVSE_IsolationMonitoringActive' and all additional
 * mandatory parameters and waits for another CableCheckReq message until the V2G_EVCC_CableCheck_Timer is equal
 * or larger than V2G_EVCC_CableCheck_Timeout.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT applies CP State B (if not already applied) and terminates the V2G communication
 * session by closing the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'.
 */
class TestBase : public virtual Test {
public:
    // === Event Handling ===
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// Whether the CableCheck stage of communication was reached
    bool cable_check_started = false;
    /// Whether there were problems processing CableCheckReq messages
    bool cable_check_failed = false;
    /// Whether CableCheck was stopped due to timers expiring
    bool cable_check_aborted = false;
    /// Whether an unexpected request was received after the timeout
    bool received_extra_request = false;
    /// Whether the control pilot state was not 'B' at the time of deviation and a transition is required
    bool validate_control_pilot = false;

    // The EV must start the V2G_EVCC_CableCheck_Timer when it sends a CableCheckReq for the first time
    // in a Communication Session. [V2G-DC-377], [V2G2-700]
    //
    // There is ambiguity regarding whether the V2G_EVCC_CableCheck_Timer shall be started before sending
    // the first CableCheckReq or after receiving the first CableCheckRes. For the purpose of validation,
    // we will accept a range of timing measurements and only fail the test if timings are unambiguous.
    //
    // V2G_EVCC_CableCheck_Timer start time will be accepted between the following events.
    //
    // From: Time the last ChargeParameterDiscoveryRes was sent
    //   1. ChargeParameterDiscoveryRes takes 0 ms to arrive
    //   2. EV processes the response and builds a CableCheckReq in 0 ms
    //   3. EV starts the V2G_EVCC_CableCheck_Timer and sends the CableCheckReq
    //   4. CableCheckReq takes remaining time to arrive at the EVSE
    // To: Time the second CableCheckReq was received
    //   1. EV sends first CableCheckReq to the EVSE
    //   2. EV receives first CableCheckRes from EVSE
    //   3. EV starts V2G_EVCC_CableCheck_Timer and sends second CableCheckReq
    //   4. CableCheckReq takes 0 ms to arrive at EVSE
    //
    // ChargeParameterDiscoveryRes-->CableCheckReq-->CableCheckRes-->CableCheckReq
    //                            |----------(timer start)----------|

    /// The earliest system time the V2G_EVCC_CableCheck_Timer could have started
    std::chrono::system_clock::time_point earliest_cable_check_timer_start; // (last ChargeParameterDiscoveryRes time)
    /// The latest system time the V2G_EVCC_CableCheck_Timer could have started
    std::chrono::system_clock::time_point latest_cable_check_timer_start; // (second CableCheckReq time)
    /// The duration of time between the second-to-last CableCheckRes and the last CableCheckReq
    std::chrono::milliseconds last_cable_check_sequence_duration{0};
    /// The maximum duration of time between any CableCheckRes and the followup CableCheckReq
    std::chrono::milliseconds max_cable_check_sequence_duration{0};
    /// The system time the last control pilot state 'B' transition occurred
    std::chrono::system_clock::time_point cp_state_b_time;
    /// The system time the last CableCheckReq message was received
    std::chrono::system_clock::time_point last_cable_check_req_time;
    /// The system time the last CableCheckRes message was sent
    std::chrono::system_clock::time_point last_cable_check_res_time;
    /// The system time the last SessionStopReq message was received
    std::chrono::system_clock::time_point last_session_stop_req_time;
    /// The system time the last SessionStopRes message was sent
    std::chrono::system_clock::time_point last_session_stop_res_time;

    /**
     * @brief Check if a time has unambiguously exceeded the cable check timeout.
     * @param tp The system time to compare against the start of cable check.
     * @return <c>true</c> if the timers have unambiguously expired, otherwise <c>false</c>.
     */
    bool is_cable_check_running_forever(const std::chrono::system_clock::time_point& tp) const;

    /**
     * Wait with a timeout for the next control pilot state 'B' event.
     * @param timeout The maximum amount of time to wait.
     * @return Whether the state 'B' was detected within the timeout.
     */
    template <typename _Rep, typename _Period>
    bool wait_for_cp_state_b(const std::chrono::duration<_Rep, _Period>& timeout) {
        std::unique_lock lock(bsp_mutex);
        return bsp_cv.wait_for(lock, timeout, [this] { return set_cp_state_b_time; });
    }

private:
    std::mutex bsp_mutex;
    std::condition_variable bsp_cv;
    bool set_cp_state_b_time = false;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_DC_VTB_CableCheck_006</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    // === Request Handling ===
    v2g_event handle_din_charge_parameter(v2g_connection* conn) override;
    v2g_event handle_din_cable_check(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;

private:
    // === Report Handling ===
    static void report_din_cable_check_req(const v2g_connection* conn,
                                           const std::chrono::system_clock::time_point& tp);
    static void report_din_cable_check_res(const v2g_connection* conn,
                                           const din_CableCheckResType* res,
                                           const std::chrono::system_clock::time_point& tp);
    static void report_din_session_stop_req(const v2g_connection* conn,
                                            const std::chrono::system_clock::time_point& tp);
    static void report_din_session_stop_res(const v2g_connection* conn,
                                            const din_SessionStopResType* res,
                                            const std::chrono::system_clock::time_point& tp);
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_DC_VTB_CableCheck_006</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    // === Request Handling ===
    v2g_event handle_iso_charge_parameter_discovery(v2g_connection* conn) override;
    v2g_event handle_iso_cable_check(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;

private:
    // === Report Handling ===
    static void report_iso_cable_check_req(const v2g_connection* conn,
                                           const std::chrono::system_clock::time_point& tp);
    static void report_iso_cable_check_res(const v2g_connection* conn,
                                           const iso2_CableCheckResType* res,
                                           const std::chrono::system_clock::time_point& tp);
    static void report_iso_session_stop_req(const v2g_connection* conn,
                                            const std::chrono::system_clock::time_point& tp);
    static void report_iso_session_stop_res(const v2g_connection* conn,
                                            const iso2_SessionStopResType* res,
                                            const std::chrono::system_clock::time_point& tp);
};

} // namespace testing::chn_cable_check_006

#endif // EVEREST_TEST_SYSTEM_CHN_CABLE_CHECK_006_HPP
