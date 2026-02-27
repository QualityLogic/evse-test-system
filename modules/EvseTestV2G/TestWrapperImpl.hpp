// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "TestWrapper.hpp"
#include "test_events.hpp"
#include "testcase.hpp"
#include "v2g.hpp"

#include <chrono>

namespace testing {

class TestWrapperImpl final : public TestWrapper {
public:
    TestWrapperImpl(v2g_connection* conn, Test* test);

    // === Lifecycle Dispatchers ===
    void dispatch_test_started() override;
    void dispatch_test_finished() override;

    // === Event Dispatchers ===
    void dispatch_connection_close_event() override;
    void dispatch_connection_close_event(const std::chrono::system_clock::time_point& tp) override;
    void dispatch_update_bsp_event(const types::board_support_common::BspEvent& bsp_event) override;
    void dispatch_update_bsp_event(const std::chrono::system_clock::time_point& tp,
                                   const types::board_support_common::BspEvent& bsp_event) override;
    void dispatch_powermeter_event(const types::powermeter::Powermeter& powermeter) override;
    void dispatch_powermeter_event(const std::chrono::system_clock::time_point& tp,
                                   const types::powermeter::Powermeter& powermeter) override;

private:
    v2g_connection* connection;
    Test* test;
};

} // namespace testing
