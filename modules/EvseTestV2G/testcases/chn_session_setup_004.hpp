// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_session_setup_004 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_SessionSetup_004</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, sends a valid SupportedAppProtocolRes message and waits until the
 * 'V2G_Msg_Timeout (par_V2G_EVCC_Msg_Timeout_SessionSetupReq)' timer has expired (no SessionSetupRes message is sent)
 * after receiving a SessionSetupReq message with SessionID '0', valid EvccID and all additional mandatory parameters.
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
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    /// Whether a SessionSetupReq message was received
    bool received_session_setup = false;
    /// Whether the control pilot state was not 'B' when timeout occurred
    bool validate_control_pilot = false;
    /// Whether an unexpected request was received after message timeout
    bool received_extra_request = false;
    /// Whether there were problems processing the SessionSetupReq message
    bool session_setup_failed = false;
    /// The system time the last SessionSetupReq message was received
    std::chrono::system_clock::time_point session_setup_req_time;
    /// The system time the last SessionStopReq message was received
    std::chrono::system_clock::time_point session_stop_req_time;
    /// The system time the last SessionStopRes message was sent
    std::chrono::system_clock::time_point session_stop_res_time;
    /// The system time the last control pilot state 'B' transition occurred
    std::chrono::system_clock::time_point cp_state_b_time;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_CMN_VTB_SessionSetup_004</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_session_setup(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_SessionSetup_004</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_session_setup(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_session_setup_004
