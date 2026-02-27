// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chn_service_detail_payment_selection_012 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_012</c> implementations defining
 * common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure, signals CP State F and sends a PaymentServiceSelectionRes message with the
 * current SessionID, ResponseCode 'OK' and all additional mandatory parameters.
 *
 * <b>Validation</b>
 *
 * Test System then checks that the SUT terminates the V2G communication session by closing the TCP connection within
 * 'par_CMN_TCP_Connection_Termination_Timeout' as soon as CP State F is applied.
 */
class TestBase : public virtual Test {
public:
    void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) override;
    void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) override;

protected:
    bool received_payment_selection = false;
    bool payment_selection_failed = false;
    bool received_extra_request = false;
    bool detected_cp_state_f = false;

    std::chrono::system_clock::time_point cp_state_f_time;

    virtual const char* get_payment_selection_msg_name() = 0;

    /**
     * Wait with a timeout for the next control pilot state 'F' event.
     * @param timeout The maximum amount of time to wait.
     * @return Whether the state 'F' was detected within the timeout.
     */
    template <typename _Rep, typename _Period>
    bool wait_for_cp_state_f(const std::chrono::duration<_Rep, _Period>& timeout);

    template <typename _Rep, typename _Period>
    bool signal_cp_state_f(v2g_connection* conn, const std::chrono::duration<_Rep, _Period>& timeout);

private:
    std::mutex bsp_mutex;
    std::condition_variable bsp_cv;
    bool set_cp_state_f_time = false;
};

/**
 * @class DinTestServer
 * @brief A <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_012</c> test implementation for the DIN 70121 protocol.
 */
class DinTestServer final : public TestBase, public DinTest {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_din_service_payment_selection(v2g_connection* conn) override;

    const char* get_payment_selection_msg_name() override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_ServiceDetailAndPaymentSelection_012</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_payment_service_selection(v2g_connection* conn) override;

    const char* get_payment_selection_msg_name() override;
};

} // namespace testing::chn_service_detail_payment_selection_012
