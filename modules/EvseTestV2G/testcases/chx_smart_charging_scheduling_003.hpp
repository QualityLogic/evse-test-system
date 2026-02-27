// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "chx_smart_charging_scheduling_base.hpp"

namespace testing::chx_smart_charging_scheduling_003 {

/**
 * @class DinTestServer
 * @brief A <c>V2GT_TC_CHX_SmartChargingScheduling_003</c> test implementation for the DIN 70121 protocol.
 *
 * <b>Procedure</b>
 *
 * Schedule via EVSE 3-5 non-zero entries in operating range of both EV/EVSE where the first entry is 0A for 1 minute.
 * Start charge and witness power transfer for 2 minutes.
 */
class DinTestServer final : public chx_smart_charging_scheduling::DinSmartChargeScheduleTest {
protected:
    int set_maximum_scheduled_power(v2g_connection* conn, short maximum_power) override;
};

/**
 * @class Iso2TestServer
 * @brief A <c>V2GT_TC_CHX_SmartChargingScheduling_003</c> test implementation for the ISO 15118-2 protocol.
 *
 * <b>Procedure</b>
 *
 * Schedule via EVSE 3-5 non-zero entries in operating range of both EV/EVSE where the first entry is 0A for 1 minute.
 * Start charge and witness power transfer for 2 minutes.
 */
class Iso2TestServer final : public chx_smart_charging_scheduling::Iso2SmartChargeScheduleTest {
protected:
    int set_maximum_scheduled_power(v2g_connection* conn, short maximum_power) override;
};

} // namespace testing::chx_smart_charging_scheduling_003
