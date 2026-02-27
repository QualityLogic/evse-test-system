// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chx_smart_charging_scheduling_002.hpp"

#include <cmath>

namespace testing::chx_smart_charging_scheduling_002 {

int DinTestServer::set_maximum_scheduled_power(v2g_connection* conn, const short maximum_power) {
    maximum_scheduled_power_0 = static_cast<short>(std::ceil(maximum_power * 0.1)); // 10% of max
    maximum_scheduled_power_1 = 0; // 0% of max
    maximum_scheduled_power_2 = static_cast<short>(std::ceil(maximum_power * 0.6)); // 60% of max
    return 0;
}

int Iso2TestServer::set_maximum_scheduled_power(v2g_connection* conn, const short maximum_power) {
    maximum_scheduled_power_0 = 1500;
    maximum_scheduled_power_1 = 0;
    maximum_scheduled_power_2 = 1400;

    // maximum_scheduled_power_0 = static_cast<short>(std::ceil(maximum_power * 0.1)); // 10% of max
    // maximum_scheduled_power_1 = 0; // 0% of max
    // maximum_scheduled_power_2 = static_cast<short>(std::ceil(maximum_power * 0.6)); // 60% of max

    return 0;
}

} // namespace testing::chx_smart_charging_scheduling_002
