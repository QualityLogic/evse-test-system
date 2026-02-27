// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_authorization_004 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_Authorization_004</c> implementations defining common fields.
 */
class TestBase : public virtual Test {
public:
    // === Event Handling ===
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    bool received_authorization = false;  // Was AuthorizationReq received?
    bool authorization_failed = false;    // Was AuthorizationRes(ResponseCode=FAILED) sent?
    bool received_extra_request = false;  // Was any request received after AuthorizationRes?

    /// Whether the control pilot state was not 'B' at the time of deviation and a transition is required.
    bool validate_control_pilot = false;
    /// The system time the last control pilot state 'B' transition occurred.
    std::chrono::system_clock::time_point cp_state_b_time;
    /// The system time the last AuthorizationRes was sent.
    std::chrono::system_clock::time_point authorization_res_time;
    /// The system time the last SessionStopReq message was received
    std::chrono::system_clock::time_point session_stop_req_time;
    /// The system time the last SessionStopRes message was sent
    std::chrono::system_clock::time_point session_stop_res_time;

    virtual const char* get_authorization_req_name() = 0;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_CMN_VTB_Authorization_004</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_contract_authentication(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;
    const char* get_authorization_req_name() override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_Authorization_004</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    // === Request Handling ===
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_authorization(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
    const char* get_authorization_req_name() override;
};

} // namespace testing::chn_authorization_004
