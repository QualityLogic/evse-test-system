// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_007_HPP
#define EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_007_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_current_demand_007 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_DC_VTB_CurrentDemand_007</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, signals CP State F, <b>waits for 0.5s</b> and sends a CurrentDemandRes
 * message with the current SessionID, ResponseCode 'OK' and all additional mandatory parameters.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT terminates the V2G communication session by closing the TCP connection within
 * 'par_CMN_TCP_Connection_Termination_Timeout' as soon as CP State F is applied. <b>NOTE: If the TCP connection is
 * closed before the corresponding request message is sent by the Test System, the send operation shall be canceled and
 * the test result shall be interpreted as successful.</b>
 */
class TestBase : public virtual Test {
public:
    // === Event Handling ===
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// Whether the CurrentDemand stage of communication was reached
    bool current_demand_started = false;
    /// Whether there were problems processing CurrentDemandReq messages
    bool current_demand_failed = false;
    /// Whether an unexpected request was received after message timeout
    bool received_extra_request = false;
    /// The system time the control pilot state 'F' transition occurred
    std::chrono::system_clock::time_point cp_state_f_time;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_DC_VTB_CurrentDemand_007</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    // === Request Handling ===
    v2g_event handle_din_current_demand(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_DC_VTB_CurrentDemand_007</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    // === Request Handling ===
    v2g_event handle_iso_current_demand(v2g_connection* conn) override;
};

} // namespace testing::chn_current_demand_007

#endif // EVEREST_TEST_SYSTEM_CHN_CURRENT_DEMAND_007_HPP
