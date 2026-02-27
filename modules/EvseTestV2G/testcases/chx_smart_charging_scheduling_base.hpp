// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include "testcase.hpp"
#include "v2g.hpp"

namespace testing::chx_smart_charging_scheduling {

class SmartChargeScheduleTest : public virtual Test {
public:
    // Event handlers
    void on_test_finished(v2g_connection* conn) override;

protected:
    long long charging_start_time = 0LL;
    long long schedule_start_time = 0LL;

    bool ev_exceeded_scheduled_pmax = false;
    bool ev_requested_nonzero_power = false;
    bool stopping_charging_session = false;
    bool gracefully_stopped_session = false;

    short maximum_scheduled_power_0 = 0;
    short maximum_scheduled_power_1 = 0;
    short maximum_scheduled_power_2 = 0;

    /**
     * Set the PMax values for all schedule intervals.
     *
     * Schedule intervals include: maximum_scheduled_power_0, maximum_scheduled_power_1, maximum_scheduled_power_2
     *
     * @param conn The v2g connection context.
     * @param maximum_power The maximum power in Watts supported by both the EVSE and EV.
     * @return 0 for success and nonzero for failure
     */
    virtual int set_maximum_scheduled_power(v2g_connection* conn, short maximum_power) = 0;

    // The maximum allowed offset between the EV and EVSE clocks
    static constexpr auto GRACE_PERIOD = std::chrono::milliseconds(V2G_TEST_SCHEDULE_ENTRY_GRACE_PERIOD);

    // The maximum duration of time between the ChargeParameterDiscoveryRes[-1] and CurrentDemandReq[0]
    // 40s for CableCheck + 7s for PreCharge + 5s for PowerDemand = 52s
    static constexpr auto MAX_SETUP_TIME = std::chrono::minutes(1);

    // The minimum amount of time the EV should spend within a single scheduled entry
    static constexpr auto MIN_TIME_IN_ENTRY = std::chrono::seconds(20);

    static constexpr auto SCHEDULE_ENTRY_START_0 = std::chrono::seconds(0);
    static constexpr auto SCHEDULE_ENTRY_START_1 = std::chrono::duration_cast<std::chrono::seconds>(SCHEDULE_ENTRY_START_0 + MAX_SETUP_TIME + GRACE_PERIOD + MIN_TIME_IN_ENTRY);
    static constexpr auto SCHEDULE_ENTRY_START_2 = std::chrono::duration_cast<std::chrono::seconds>(SCHEDULE_ENTRY_START_1 + GRACE_PERIOD + MIN_TIME_IN_ENTRY + GRACE_PERIOD);
    static constexpr auto SCHEDULE_ENTRY_DURATION = std::chrono::hours(1);
    static constexpr auto TARGET_CHARGING_DURATION = (SCHEDULE_ENTRY_START_1 - SCHEDULE_ENTRY_START_0) + GRACE_PERIOD + MIN_TIME_IN_ENTRY;
};

class DinSmartChargeScheduleTest : public SmartChargeScheduleTest, public DinTest {
protected:
    // Report builders
    static void report_charge_parameter_req(const v2g_connection* conn, std::chrono::system_clock::time_point tp);
    static void report_charge_parameter_res(const v2g_connection* conn);
    static void report_current_demand_req(const v2g_connection* conn);
    static void report_current_demand_res(const v2g_connection* conn);

    // Request handlers
    v2g_event handle_din_charge_parameter(v2g_connection* conn) override;
    v2g_event handle_din_power_delivery(v2g_connection* conn) override;
    v2g_event handle_din_current_demand(v2g_connection* conn) override;
    v2g_event handle_din_session_stop(v2g_connection* conn) override;

    // Utilities
    /**
     * Get the maximum power limit mutually supported by the EV and EVSE in Watts.
     * @param req A pointer to the ChargeParameterDiscoveryReqType message.
     * @param res A Pointer to the ChargeParameterDiscoveryResType message.
     * @return The maximum power limit in Watts.
     */
    static short get_maximum_power_limit(const din_ChargeParameterDiscoveryReqType* req,
                                         const din_ChargeParameterDiscoveryResType* res);

private:
    // The calculated target power in Watts of the last CurrentDemandReq
    double last_target_power = 0;

    // EVSE status
    int last_ev_ready = 1;
    din_DC_EVErrorCodeType last_ev_error_code = din_DC_EVErrorCodeType_NO_ERROR;
    din_DC_EVSEStatusCodeType last_evse_status_code = din_DC_EVSEStatusCodeType_EVSE_Ready;
    din_EVSENotificationType last_evse_notification = din_EVSENotificationType_None;
};

class Iso2SmartChargeScheduleTest : public SmartChargeScheduleTest, public Iso2Test {
public:
    // Event handlers
    void on_powermeter_event(v2g_connection* conn, const PowermeterEvent& event) override;

protected:
    // Report builders
    static void report_charge_parameter_req(const v2g_connection* conn, std::chrono::system_clock::time_point tp);
    static void report_charge_parameter_res(const v2g_connection* conn);
    static void report_current_demand_req(const v2g_connection* conn);
    static void report_current_demand_res(const v2g_connection* conn);

    // Request handlers
    v2g_event handle_iso_charge_parameter_discovery(v2g_connection* conn) override;
    v2g_event handle_iso_power_delivery(v2g_connection* conn) override;
    v2g_event handle_iso_charging_status(v2g_connection* conn) override;
    v2g_event handle_iso_current_demand(v2g_connection* conn) override;
    v2g_event handle_iso_session_stop(v2g_connection* conn) override;

    // Utilities
    /**
     * Get the maximum power limit mutually supported by the EV and EVSE in Watts.
     * @param req A pointer to the ChargeParameterDiscoveryReqType message.
     * @param res A Pointer to the ChargeParameterDiscoveryResType message.
     * @return The maximum power limit in Watts.
     */
    static short get_maximum_power_limit(const iso2_ChargeParameterDiscoveryReqType* req,
                                         const iso2_ChargeParameterDiscoveryResType* res);

    /**
     * Set the latest powermeter power reading in Watts.
     * @param power latest powermeter power reading in Watts.
     */
    void set_power_reading(float power);

    /**
     * Get the latest powermeter power reading in Watts and clear its value.
     * @return latest powermeter power reading in Watts.
     */
    std::optional<float> get_and_clear_power_reading();

private:
    // The calculated target power in Watts of the last CurrentDemandReq
    double last_target_power = 0;

    // EVSE status
    int last_ev_ready = 1;
    iso2_DC_EVErrorCodeType last_ev_error_code = iso2_DC_EVErrorCodeType_NO_ERROR;
    iso2_DC_EVSEStatusCodeType last_evse_status_code = iso2_DC_EVSEStatusCodeType_EVSE_Ready;
    iso2_EVSENotificationType last_evse_notification = iso2_EVSENotificationType_None;

    // PowerMeter readings
    std::mutex powermeter_mutex{};      // locked whenever powermeter fields are assigned/read
    float power_reading_W = 0.0;        // most recent power measurement in Watts
    bool power_reading_updated = false; // set 'true' every time 'power_reading_W' is set
};

} // namespace testing::chx_smart_charging_scheduling
