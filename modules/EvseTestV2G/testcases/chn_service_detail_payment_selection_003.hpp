// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_service_detail_payment_selection_003 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_003</c> implementations defining
 * common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure and turns off the oscillator before sending a PaymentServiceSelectionRes
 * message with the current SessionID, ResponseCode 'FAILED' and all additional mandatory parameters.
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
    bool received_payment_selection = false;
    bool received_session_stop = false;
    bool received_extra_request = false;
    bool payment_selection_failed = false;
    bool validate_control_pilot = false;

    std::chrono::system_clock::time_point payment_selection_res_time;
    std::chrono::system_clock::time_point session_stop_req_time;
    std::chrono::system_clock::time_point session_stop_res_time;
    std::chrono::system_clock::time_point cp_state_b_time;

    virtual const char* get_payment_selection_msg_name() = 0;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_003</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_service_payment_selection(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;

    const char* get_payment_selection_msg_name() override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_003</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_payment_service_selection(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;

    const char* get_payment_selection_msg_name() override;
};

} // namespace testing::chn_service_detail_payment_selection_003
