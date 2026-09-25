// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_authorization_009 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_Authorization_009</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, sends continuously an AuthorizationRes message with the current SessionID,
 * ResponseCode 'OK', EVSEProcessing 'Ongoing' and all additional mandatory parameters and waits for another
 * AuthorizationReq message with the current SessionID and all additional mandatory parameters until the
 * V2G_EVCC_Ongoing_Timer is equal or larger than V2G_EVCC_Ongoing_Timeout.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT applies CP State B (if not already applied) and terminates the V2G communication
 * session by closing the TCP connection within 'par_CMN_TCP_Connection_Termination_Timeout'.
 */
class TestBase : public virtual Test {
public:
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    int64_t auth_start_time = 0LL;       // When was the first authorization request received?
    int64_t last_auth_req_time = 0LL;    // When was the last authorization request received?
    int64_t last_auth_res_time = 0LL;    // When was the last authorization response sent?

    // network/followup request latency
    int64_t sequence_time = 0LL;

    bool received_authorization = false; // Did the EV ever send an authorization request?
    bool received_extra_request = false; // Did the EV send a request beyond the authorization state?
    bool authorization_aborted = false;  // Was authorization canceled due to the EV never giving up?
    bool authorization_failed = false;   // Did authorization fail due to problems beyond timeouts?
    bool validate_control_pilot = false; // Should the control pilot state be validated at connection termination?

    std::chrono::system_clock::time_point cp_state_b_time; // When was the last control pilot state 'B' transition?
    std::chrono::system_clock::time_point session_stop_req_time;
    std::chrono::system_clock::time_point session_stop_res_time;

    virtual const char* get_authorization_req_name() = 0;

    /**
     * @brief Check if a time has unambiguously exceeded the authorization timeout.
     * @param time The monotonic time to compare against the authorization timers.
     * @return <c>true</c> if the timers have unambiguously expired, otherwise <c>false</c>.
     */
    bool is_auth_running_forever(int64_t time) const;

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
 * @brief A <c>TC_EVCC_CMN_VTB_Authorization_009</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_contract_authentication(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
    const char* get_authorization_req_name() override;
};

} // namespace testing::chn_authorization_009
