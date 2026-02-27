// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

#define SERVICE_DETAIL_013_SERVICE_ID 62002

namespace testing::chn_service_detail_013 {

/**
 * @class TestBase
 * @brief A base class for all <c>TC_EVCC_CMN_VTB_ServiceDetail_013</c> implementations defining common fields.
 *
 * <b>Procedure</b>
 *
 * Test System executes GoodCase procedure and sends a valid PaymentServiceSelectionRes message instead of
 * ServiceDetailRes message.
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
    bool received_service_detail = false; // Was ServiceDetailReq received?
    bool received_extra_request = false;  // Was any request received after PaymentServiceSelectionRes?
    bool service_detail_failed = false;   // Was ServiceDetailRes(ResponseCode=FAILED) sent?
    bool validate_control_pilot = false;  // Do we need to validate a control pilot state transition?

    std::chrono::system_clock::time_point payment_selection_res_time;
    std::chrono::system_clock::time_point session_stop_req_time;
    std::chrono::system_clock::time_point session_stop_res_time;
    std::chrono::system_clock::time_point cp_state_b_time;
};

/**
 * @class Iso2TestServer
 * @brief A <c>TC_EVCC_CMN_VTB_ServiceDeatil_013</c> test implementation for the ISO 15118-2 protocol.
 */
class Iso2TestServer final : public TestBase, public Iso2Test {
public:
    v2g_event handle_request(v2g_connection* conn) override;

protected:
    v2g_event handle_iso_service_discovery(v2g_connection* conn) override;
    v2g_event handle_iso_service_detail(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;
};

} // namespace testing::chn_service_detail_013
