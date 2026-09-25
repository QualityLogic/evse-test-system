// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_002_HPP
#define EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_002_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_current_demand_002 {

/**
 * @class TestBase
 * @brief A base class for all <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_002</c> implementations defining common fields.
 */
class TestBase : public virtual Test {
public:
    // === Event Handling ===
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// Whether the CurrentDemand stage of communication was reached
    bool current_demand_started = false;
    /// Whether there were problems processing CurrentDemandReq messages
    bool current_demand_failed = false;
    /// Whether an unexpected request was received after CurrentDemandRes(ResponseCode=FAILED)
    bool received_extra_request = false;
    /// Whether the control pilot state was not 'B' at the time of deviation and a transition is required
    bool validate_control_pilot = false;

    /// The system time the last control pilot state 'B' transition occurred
    std::chrono::system_clock::time_point cp_state_b_time;
    /// The system time the last CurrentDemandRes was sent
    std::chrono::system_clock::time_point current_demand_res_time;
    /// The system time the last SessionStopRes was sent
    std::chrono::system_clock::time_point session_stop_res_time;

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
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_002</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_current_demand(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_002</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_current_demand(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_current_demand_002

#endif // EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_002_HPP
