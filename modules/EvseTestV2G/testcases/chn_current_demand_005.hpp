// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_005_HPP
#define EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_005_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_current_demand_005 {

/**
 * @class TestBase
 * @brief A base class for all <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_005</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, sends valid PowerDeliveryRes message, and waits until the 'V2G_Msg_Timeout
 * (par_V2G_EVCC_Msg_Timeout_CurrentDemandReq)' timer has expired (no CurrentDemandRes message is sent) after receiving
 * a CurrentDemandReq message with the current SessionID, target voltage and current, and all additional mandatory
 * parameters.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT applies CP State B within 'par_EVCC_StateB_Shutdown_Timeout' (if not already
 * applied) and terminates the V2G communication session by closing the TCP connection within
 * 'par_CMN_TCP_Connection_Termination_Timeout'. If a SessionStopReq message with the current SessionID and all
 * additional mandatory parameters was received before, Test System sends a valid SessionStopRes message, restarts the
 * timer 'par_CMN_TCP_Connection_Termination_Timeout', and waits for TCP connection termination.
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
    /// Whether an unexpected request was received after message timeout
    bool received_extra_request = false;
    /// Whether the control pilot state was not 'B' at the time of deviation and a transition is required
    bool validate_control_pilot = false;
    /// The system time the last control pilot state 'B' transition occurred
    std::chrono::system_clock::time_point cp_state_b_time;
    /// The system time the last CurrentDemandReq message was received
    std::chrono::system_clock::time_point current_demand_req_time;
    /// The system time the last SessionStopReq message was received
    std::chrono::system_clock::time_point session_stop_req_time;
    /// The system time the last SessionStopRes message was sent
    std::chrono::system_clock::time_point session_stop_res_time;
};

/**
 * @class DinTestServer
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_005</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_power_delivery(v2g_connection* conn) override;
    v2g_event handle_din_current_demand(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>CharIN_TC_EVCC_DC_VTB_CurrentDemand_005</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_power_delivery(v2g_connection* conn) override;
    v2g_event handle_iso_current_demand(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_current_demand_005

#endif // EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_005_HPP
