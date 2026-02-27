// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef CHN_SESSION_SETUP_007_HPP
#define CHN_SESSION_SETUP_007_HPP

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_session_setup_007 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_SessionSetup_007</c> implementations defining common fields.
 */
class TestBase : public virtual Test {
public:
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    bool received_session_setup = false; // Was SessionSetupReq received?
    bool session_setup_failed = false;   // Was SessionSetupRes(ResponseCode=FAILED) sent?
    bool received_extra_request = false; // Was any request received after control pilot state 'F'?

    /// The system time the last control pilot state 'F' transition occurred.
    std::chrono::system_clock::time_point cp_state_f_time;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_CMN_VTB_SessionSetup_007</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:

    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_session_setup(v2g_connection* conn) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_SessionSetup_007</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:

    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_session_setup(v2g_connection* conn) override;
};

} // namespace testing::chn_session_setup_007

#endif //CHN_SESSION_SETUP_007_HPP
