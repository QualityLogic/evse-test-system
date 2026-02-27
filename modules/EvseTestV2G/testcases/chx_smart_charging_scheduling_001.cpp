// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chx_smart_charging_scheduling_001.hpp"

#include "tools.hpp"

#include <cmath>

namespace testing::chx_smart_charging_scheduling_001 {

int DinTestServer::set_maximum_scheduled_power(v2g_connection* conn, const short maximum_power) {
    maximum_scheduled_power_0 = static_cast<short>(std::ceil(maximum_power * 0.1)); // 10% of max
    maximum_scheduled_power_1 = static_cast<short>(std::ceil(maximum_power * 0.4)); // 40% of max
    maximum_scheduled_power_2 = static_cast<short>(std::ceil(maximum_power * 0.6)); // 60% of max
    return 0;
}

int Iso2TestServer::set_maximum_scheduled_power(v2g_connection* conn, const short maximum_power) {
    if (conn->ctx->is_dc_charger) {

    } else {
        const auto pv = &conn->ctx->evse_v2g_data.evse_nominal_voltage;
        const auto nominal_voltage = calc_physical_value(pv->Value, pv->Multiplier);
        const auto max_current = maximum_power / nominal_voltage;
    }

    maximum_scheduled_power_0 = 1800;
    maximum_scheduled_power_1 = 1400;
    maximum_scheduled_power_2 = 1600;

    // maximum_scheduled_power_0 = static_cast<short>(std::ceil(maximum_power * 0.1)); // 10% of max
    // maximum_scheduled_power_1 = static_cast<short>(std::ceil(maximum_power * 0.4)); // 40% of max
    // maximum_scheduled_power_2 = static_cast<short>(std::ceil(maximum_power * 0.6)); // 60% of max

    return 0;
}

} // namespace testing::chx_smart_charging_scheduling_001
