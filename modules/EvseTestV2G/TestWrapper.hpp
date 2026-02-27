// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include <chrono>
#include <generated/types/board_support_common.hpp>
#include <generated/types/powermeter.hpp>

namespace testing {

class TestWrapper {
public:
    virtual ~TestWrapper() = default;

    // === Lifecycle Dispatchers ===
    virtual void dispatch_test_started() = 0;
    virtual void dispatch_test_finished() = 0;

    // === Event Dispatchers ===
    virtual void dispatch_connection_close_event() = 0;
    virtual void dispatch_connection_close_event(const std::chrono::system_clock::time_point& tp) = 0;
    virtual void dispatch_update_bsp_event(const types::board_support_common::BspEvent& bsp_event) = 0;
    virtual void dispatch_update_bsp_event(const std::chrono::system_clock::time_point& tp,
                                           const types::board_support_common::BspEvent& bsp_event) = 0;
    virtual void dispatch_powermeter_event(const types::powermeter::Powermeter& powermeter) = 0;
    virtual void dispatch_powermeter_event(const std::chrono::system_clock::time_point& tp,
                                           const types::powermeter::Powermeter& powermeter) = 0;
};

} // namespace testing
