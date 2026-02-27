// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include <chrono>
#include <generated/types/board_support_common.hpp>

namespace testing {

struct Event {
    /// The system time the event occurred.
    std::chrono::system_clock::time_point timestamp;
};

struct ConnectionCloseEvent : Event {
};

struct UpdateBspEvent : Event {
    /// The updated BSP state
    types::board_support_common::Event bsp_event;
};

struct PowermeterEvent : Event {
    /// The updated powermeter readings.
    types::powermeter::Powermeter powermeter;
};

} // namespace testing
