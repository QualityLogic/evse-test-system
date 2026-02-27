// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "chx_smart_charging_scheduling_base.hpp"

#include "log.hpp"
#include "report_tools.hpp"
#include "tools.hpp"
#include "v2g_ctx.hpp"

using namespace types::evse_test_common;

namespace testing::chx_smart_charging_scheduling {

#pragma region COMMON

static void log_grace_period_last_interval() {
    dlog(DLOG_LEVEL_DEBUG, "EV Target Power exceeds PMax but is within grace period of last scheduled interval");
}

static void log_grace_period_next_interval() {
    dlog(DLOG_LEVEL_DEBUG, "EV Target Power exceeds PMax but is within grace period of next scheduled interval");
}

//=============================================
//             Event Handlers
//=============================================

void SmartChargeScheduleTest::on_test_finished(v2g_connection* conn) {
    const auto test_data = &conn->ctx->test_data;
    const bool schedule_created = schedule_start_time > 0;
    const bool charging_started = charging_start_time > 0;

    // [PreCondition] The EVSE must send a ChargeParameterDiscoveryRes containing the charging schedule
    if (not schedule_created) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EVSE never communicated a charge schedule");
        return;
    }

    // [PreCondition] The EVSE must send a PowerDeliveryRes(OK) in response to a PowerDeliveryReq(Start)
    if (not charging_started) {
        test_data->outcome = TestOutcome::PreconditionsNotMet;
        test_data->errors.emplace_back("EV never started charging");
        return;
    }

    // [PassCriteria] The EV must not exceed the scheduled PMax limit for the appropriate time interval
    if (ev_exceeded_scheduled_pmax) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV exceeded scheduled PMax limits");
    }

    // [PassCriteria] The EV must request at non-zero power during the charging session
    if (not ev_requested_nonzero_power) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV never requested significant power");
    }

    // [PassCriteria] The EV must charge until gracefully stopped by the EVSE
    if (not stopping_charging_session) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("EV stopped charging early");
    }

    // [PassCriteria] The charging session must end with a SessionStopRes(OK)
    if (not gracefully_stopped_session) {
        test_data->outcome = TestOutcome::PassCriteriaNotMet;
        test_data->errors.emplace_back("Charging session did not gracefully terminate");
    }

    if (test_data->outcome != TestOutcome::PassCriteriaNotMet) {
        test_data->outcome = TestOutcome::PassCriteriaMet;
    }
}

#pragma endregion COMMON

#pragma region DIN_70121

//=============================================
//             Report Builders
//=============================================

void DinSmartChargeScheduleTest::report_charge_parameter_req(const v2g_connection* conn,
                                                             const std::chrono::system_clock::time_point tp) {
    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.ChargeParameterDiscoveryReq;
    std::vector<types::test_report::MessageField> message_fields;

    if (req->AC_EVChargeParameter_isUsed) {
        const auto max_current = din_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMaxCurrent, 1);
        const auto max_voltage = din_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMaxVoltage, 1);
        const auto min_current = din_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMinCurrent, 1);
        message_fields.push_back({"EVMaxCurrent", max_current});
        message_fields.push_back({"EVMaxVoltage", max_voltage});
        message_fields.push_back({"EVMinCurrent", min_current});
    }

    if (req->DC_EVChargeParameter_isUsed) {
        const auto max_current = din_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumCurrentLimit, 1);
        const auto max_voltage = din_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumVoltageLimit, 1);
        message_fields.push_back({"EVMaximumCurrentLimit", max_current});
        message_fields.push_back({"EVMaximumVoltageLimit", max_voltage});
        if (req->DC_EVChargeParameter.EVMaximumPowerLimit_isUsed) {
            const auto max_power = din_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumPowerLimit, 1);
            message_fields.push_back({"EVMaximumPowerLimit", max_power});
        }
    }

    report_din_request(conn, V2G_CHARGE_PARAMETER_DISCOVERY_MSG, {
        .message_fields = message_fields,
        .metadata { timepoint_to_iso8601_str(tp) },
    });
}

void DinSmartChargeScheduleTest::report_charge_parameter_res(const v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.ChargeParameterDiscoveryRes;
    std::vector<types::test_report::MessageField> message_fields;

    const auto evse_processing_str = din_EVSEProcessingType_to_string(res->EVSEProcessing);
    message_fields.push_back({"EVSEProcessing", evse_processing_str});

    if (res->SAScheduleList_isUsed) {
        for (int schedule_index = 0; schedule_index < res->SAScheduleList.SAScheduleTuple.arrayLen; ++schedule_index) {
            const auto schedule = &res->SAScheduleList.SAScheduleTuple.array[schedule_index];

            // SAScheduleTupleID
            const auto tuple_id_field_name = fmt::format("SAScheduleList.SAScheduleTuple[{}].SAScheduleTupleID",
                schedule_index);
            const auto tuple_id_field_value = std::to_string(schedule->SAScheduleTupleID);
            message_fields.push_back({tuple_id_field_name, tuple_id_field_value});

            for (int entry_index = 0; entry_index < schedule->PMaxSchedule.PMaxScheduleEntry.arrayLen; ++entry_index) {
                const auto entry = &schedule->PMaxSchedule.PMaxScheduleEntry.array[entry_index];
                const auto entry_field_prefix = fmt::format(
                    "SAScheduleList.SAScheduleTuple[{}].PMaxSchedule.PMaxScheduleEntry[{}]",
                    schedule_index, entry_index);

                // RelativeTimeInterval
                if (entry->RelativeTimeInterval_isUsed) {
                    // Start
                    const auto start_field_name = entry_field_prefix + ".RelativeTimeInterval.start";
                    const auto start_field_value = std::to_string(entry->RelativeTimeInterval.start);
                    message_fields.push_back({start_field_name, start_field_value});

                    // Duration
                    if (entry->RelativeTimeInterval.duration_isUsed) {
                        const auto duration_field_name = entry_field_prefix + ".RelativeTimeInterval.duration";
                        const auto duration_field_value = std::to_string(entry->RelativeTimeInterval.duration);
                        message_fields.push_back({duration_field_name, duration_field_value});
                    }
                }

                // PMax
                const auto pmax_field_name = entry_field_prefix + ".PMax";
                const auto pmax_field_value = std::to_string(entry->PMax) + "W";
                message_fields.push_back({pmax_field_name, pmax_field_value});
            }
        }
    }

    if (res->AC_EVSEChargeParameter_isUsed) {
        const auto max_current = din_PhysicalValueType_to_string(res->AC_EVSEChargeParameter.EVSEMaxCurrent, 1);
        const auto max_voltage = din_PhysicalValueType_to_string(res->AC_EVSEChargeParameter.EVSEMaxVoltage, 1);
        const auto min_current = din_PhysicalValueType_to_string(res->AC_EVSEChargeParameter.EVSEMinCurrent, 1);
        message_fields.push_back({"EVSEMaxCurrent", max_current});
        message_fields.push_back({"EVSEMaxVoltage", max_voltage});
        message_fields.push_back({"EVSEMinCurrent", min_current});
    }

    if (res->DC_EVSEChargeParameter_isUsed) {
        const auto max_current = din_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumCurrentLimit, 1);
        const auto max_voltage = din_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumVoltageLimit, 1);
        message_fields.push_back({"EVSEMaximumCurrentLimit", max_current});
        message_fields.push_back({"EVSEMaximumVoltageLimit", max_voltage});
        if (res->DC_EVSEChargeParameter.EVSEMaximumPowerLimit_isUsed) {
            const auto max_power = din_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumPowerLimit, 1);
            message_fields.push_back({"EVSEMaximumPowerLimit", max_power});
        }
    }

    report_din_response(conn, V2G_CHARGE_PARAMETER_DISCOVERY_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
    });
}

void DinSmartChargeScheduleTest::report_current_demand_req(const v2g_connection* conn) {
    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.CurrentDemandReq;
    std::vector<types::test_report::MessageField> message_fields;

    if (not req->DC_EVStatus.EVReady) {
        message_fields.push_back({"EVReady", "false"});
    }

    if (req->DC_EVStatus.EVErrorCode != din_DC_EVErrorCodeType_NO_ERROR) {
        const auto error_code = din_DC_EVErrorCodeType_to_string(req->DC_EVStatus.EVErrorCode);
        message_fields.push_back({"DC_EVErrorCode", error_code});
    }

    const auto target_current = din_PhysicalValueType_to_string(req->EVTargetCurrent, 1);
    const auto target_voltage = din_PhysicalValueType_to_string(req->EVTargetVoltage, 1);
    const auto target_current_value = calc_physical_value(req->EVTargetCurrent.Value, req->EVTargetCurrent.Multiplier);
    const auto target_voltage_value = calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier);
    const auto target_power_value = target_current_value * target_voltage_value;
    const auto target_power = double_to_rounded_string(target_power_value, 1) + "W";

    message_fields.push_back({"EVTargetCurrent", target_current});
    message_fields.push_back({"EVTargetVoltage", target_voltage});
    message_fields.push_back({"EVTargetPower", target_power});

    report_din_request(conn, V2G_CURRENT_DEMAND_MSG, {
        .message_fields = message_fields,
    });
}

void DinSmartChargeScheduleTest::report_current_demand_res(const v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.CurrentDemandRes;
    std::vector<types::test_report::MessageField> message_fields;

    if (res->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_Ready) {
        const auto status_code = din_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode);
        message_fields.push_back({"DC_EVSEStatusCode", status_code});
    }

    if (res->DC_EVSEStatus.EVSENotification != din_EVSENotificationType_None) {
        const auto notification = din_EVSENotificationType_to_string(res->DC_EVSEStatus.EVSENotification);
        message_fields.push_back({"EVSENotification", notification});
    }

    const auto present_current = din_PhysicalValueType_to_string(res->EVSEPresentCurrent, 1);
    const auto present_voltage = din_PhysicalValueType_to_string(res->EVSEPresentVoltage, 1);
    const auto present_current_value = calc_physical_value(res->EVSEPresentCurrent.Value, res->EVSEPresentCurrent.Multiplier);
    const auto present_voltage_value = calc_physical_value(res->EVSEPresentVoltage.Value, res->EVSEPresentVoltage.Multiplier);
    const auto present_power_value = present_current_value * present_voltage_value;
    const auto present_power = double_to_rounded_string(present_power_value, 1) + "W";

    message_fields.push_back({"EVSEPresentCurrent", present_current});
    message_fields.push_back({"EVSEPresentVoltage", present_voltage});
    message_fields.push_back({"EVSEPresentPower", present_power});

    report_din_response(conn, V2G_CURRENT_DEMAND_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
    });
}

//=============================================
//             Request Handlers
//=============================================

v2g_event DinSmartChargeScheduleTest::handle_din_charge_parameter(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.ChargeParameterDiscoveryReq;
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.ChargeParameterDiscoveryRes;

    // Note the current time the request arrived so we can accurately report the request if the
    // response has an EVSEProcessing value of Finished
    const auto req_arrival_time = std::chrono::system_clock::now();

    auto next_event = DinTest::handle_din_charge_parameter(conn);

    // Only overwrite the PMax schedule of the last ChargeParameterDiscoveryRes if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {
        if (res->EVSEProcessing == din_EVSEProcessingType_Finished) {

            const auto schedule = &res->SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule;
            const auto maximum_power_limit = get_maximum_power_limit(req, res);

            // Calculate the maximum power allowed for each schedule entry
            if (set_maximum_scheduled_power(conn, maximum_power_limit) != 0) {
                // If there were problems creating a power schedule, we cannot proceed.
                dlog(DLOG_LEVEL_INFO, "Failed to create a PMax schedule for the test.");
                res->ResponseCode = din_responseCodeType_FAILED;
                next_event = DinTest::din_validate_response_code(&res->ResponseCode, conn);
            }
            // Populate the schedule entries
            else {
                // Define the first schedule entry
                init_din_PMaxScheduleEntryType(&schedule->PMaxScheduleEntry.array[0]);
                init_din_RelativeTimeIntervalType(&schedule->PMaxScheduleEntry.array[0].RelativeTimeInterval);
                schedule->PMaxScheduleEntry.array[0].PMax = maximum_scheduled_power_0;
                schedule->PMaxScheduleEntry.array[0].RelativeTimeInterval_isUsed = 1u;
                schedule->PMaxScheduleEntry.array[0].RelativeTimeInterval.start =
                    duration_cast<seconds>(SCHEDULE_ENTRY_START_0).count();

                // Define the second schedule entry
                init_din_PMaxScheduleEntryType(&schedule->PMaxScheduleEntry.array[1]);
                init_din_RelativeTimeIntervalType(&schedule->PMaxScheduleEntry.array[1].RelativeTimeInterval);
                schedule->PMaxScheduleEntry.array[1].PMax = maximum_scheduled_power_1;
                schedule->PMaxScheduleEntry.array[1].RelativeTimeInterval_isUsed = 1u;
                schedule->PMaxScheduleEntry.array[1].RelativeTimeInterval.start =
                    duration_cast<seconds>(SCHEDULE_ENTRY_START_1).count();

                // Define the third schedule entry
                init_din_PMaxScheduleEntryType(&schedule->PMaxScheduleEntry.array[2]);
                init_din_RelativeTimeIntervalType(&schedule->PMaxScheduleEntry.array[2].RelativeTimeInterval);
                schedule->PMaxScheduleEntry.array[2].PMax = maximum_scheduled_power_2;
                schedule->PMaxScheduleEntry.array[2].RelativeTimeInterval_isUsed = 1u;
                schedule->PMaxScheduleEntry.array[2].RelativeTimeInterval.duration_isUsed = 1u;
                schedule->PMaxScheduleEntry.array[2].RelativeTimeInterval.duration =
                    duration_cast<seconds>(SCHEDULE_ENTRY_DURATION).count();
                schedule->PMaxScheduleEntry.array[2].RelativeTimeInterval.start =
                    duration_cast<seconds>(SCHEDULE_ENTRY_START_2).count();

                // The schedule now contains 3 schedule entries
                schedule->PMaxScheduleEntry.arrayLen = 3;

                dlog(DLOG_LEVEL_INFO, "EVSE Max Power: %dW, Schedule: [%dW, %dW, %dW]",
                     static_cast<int>(maximum_power_limit),
                     static_cast<int>(maximum_scheduled_power_0),
                     static_cast<int>(maximum_scheduled_power_1),
                     static_cast<int>(maximum_scheduled_power_2));

                // This additionally indicates that the PMax schedule was sent
                schedule_start_time = getmonotonictime();
            }
        }
    }

    // Include the final ChargeParameterDiscovery Req/Res in the test report
    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if (res->EVSEProcessing == din_EVSEProcessingType_Finished) {
            report_charge_parameter_req(conn, req_arrival_time);
            report_charge_parameter_res(conn);
        }
    }

    return next_event;
}

v2g_event DinSmartChargeScheduleTest::handle_din_power_delivery(v2g_connection* conn) {
    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.PowerDeliveryReq;
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.PowerDeliveryRes;

    report_din_request(conn, V2G_POWER_DELIVERY_MSG, {
        .message_fields = {
            {"ReadyToChargeState", req->ReadyToChargeState ? "true" : "false"},
        }
    });

    const auto next_event = DinTest::handle_din_power_delivery(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {
        // Note the time the EV requests to start charging so we know how long charging has
        // been running during current demand.
        if (req->ReadyToChargeState and charging_start_time == 0) {
            charging_start_time = getmonotonictime();
        }
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_din_response(conn, V2G_POWER_DELIVERY_MSG, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

v2g_event DinSmartChargeScheduleTest::handle_din_current_demand(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto req = &conn->exi_in.dinEXIDocument->V2G_Message.Body.CurrentDemandReq;
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.CurrentDemandRes;

    const auto ev_target_current = calc_physical_value(req->EVTargetCurrent.Value, req->EVTargetCurrent.Multiplier);
    const auto ev_target_voltage = calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier);
    const auto ev_target_power = ev_target_current * ev_target_voltage;
    bool report_response = false;

    if ((std::abs(ev_target_power - last_target_power) > 1) or
        (req->DC_EVStatus.EVErrorCode != last_ev_error_code) or
        (req->DC_EVStatus.EVReady != last_ev_ready)) {

        last_target_power = ev_target_power;
        last_ev_error_code = req->DC_EVStatus.EVErrorCode;
        last_ev_ready = req->DC_EVStatus.EVReady;
        report_response = true;

        report_current_demand_req(conn);
    }

    const auto next_event = DinTest::handle_din_current_demand(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < din_responseCodeType_FAILED) {

        const auto now = getmonotonictime();
        const auto charging_elapsed_time = milliseconds(now - charging_start_time);
        const auto schedule_elapsed_time = milliseconds(now - schedule_start_time);
        const auto grace_period = milliseconds(V2G_TEST_SCHEDULE_ENTRY_GRACE_PERIOD);

        short last_scheduled_pmax = -1; // previous pmax
        short this_scheduled_pmax = -1; // current pmax
        short next_scheduled_pmax = -1; // upcoming pmax
        std::chrono::seconds last_schedule_start;
        std::chrono::seconds next_schedule_start;

        // In first scheduled time interval?
        if (schedule_elapsed_time < SCHEDULE_ENTRY_START_1) {
            this_scheduled_pmax = maximum_scheduled_power_0;
            next_schedule_start = SCHEDULE_ENTRY_START_1;
            next_scheduled_pmax = maximum_scheduled_power_1;
        }
        // In second scheduled time interval?
        else if (schedule_elapsed_time < SCHEDULE_ENTRY_START_2) {
            last_schedule_start = SCHEDULE_ENTRY_START_0;
            last_scheduled_pmax = maximum_scheduled_power_0;
            this_scheduled_pmax = maximum_scheduled_power_1;
            next_schedule_start = SCHEDULE_ENTRY_START_2;
            next_scheduled_pmax = maximum_scheduled_power_2;
        }
        // In third scheduled time interval?
        else if (schedule_elapsed_time <= SCHEDULE_ENTRY_START_2 + SCHEDULE_ENTRY_DURATION) {
            last_schedule_start = SCHEDULE_ENTRY_START_1;
            last_scheduled_pmax = maximum_scheduled_power_1;
            this_scheduled_pmax = maximum_scheduled_power_2;
        }

        const auto fmt_charging_elapsed = format_duration(charging_elapsed_time);
        const auto fmt_schedule_elapsed = format_duration(schedule_elapsed_time);
        dlog(DLOG_LEVEL_INFO, "Charging time: %s, Schedule time: %s, Present power: %.1fW, Scheduled PMax: %dW",
             fmt_charging_elapsed.c_str(), fmt_schedule_elapsed.c_str(), ev_target_power,
             static_cast<int>(this_scheduled_pmax));

        // Did the EV exceed the PMax of the current scheduled time interval?
        if (ev_target_power > this_scheduled_pmax) {
            // Is the EV within the grace period of the last scheduled time interval?
            if (ev_target_power <= last_scheduled_pmax and
                schedule_elapsed_time <= last_schedule_start + SCHEDULE_ENTRY_DURATION + grace_period) {
                // We are within the grace period of the last PMax Schedule Entry
                log_grace_period_last_interval();
            }
            // Is the EV within the grace period of the next scheduled time interval?
            else if (ev_target_power <= next_scheduled_pmax and
                     schedule_elapsed_time >= next_schedule_start - grace_period) {
                // We are within the grace period of the next PMax Schedule Entry
                log_grace_period_next_interval();
            }
            // Handle the EV exceeding the PMax of all applicable scheduled time intervals
            else if (!ev_exceeded_scheduled_pmax) {
                ev_exceeded_scheduled_pmax = true;
                const auto max_power = this_scheduled_pmax < 0 ? 0 : this_scheduled_pmax;
                const auto error_message = dc_scheduled_power_exceeded_error(ev_target_power, max_power);
                conn->ctx->test_data.errors.push_back(error_message);
                dlog(DLOG_LEVEL_INFO, error_message.c_str());
            }
        }

        // Check if the EV requested non-zero power for test pass-criteria validation
        if (!ev_requested_nonzero_power and ev_target_power > 1) {
            ev_requested_nonzero_power = true;
        }

        // Update the EVSE Maximum Power Limit in the response to reflect the PMax value
        if (res->EVSEMaximumPowerLimit_isUsed) {
            const auto max_power = this_scheduled_pmax < 0 ? 0 : this_scheduled_pmax;
            iso2_PhysicalValueType iso2_pv;
            init_physical_value(&iso2_pv, iso2_unitSymbolType_W);
            populate_physical_value(&iso2_pv, max_power, iso2_pv.Unit);
            res->EVSEMaximumPowerLimit.Value = iso2_pv.Value;
            res->EVSEMaximumPowerLimit.Multiplier = iso2_pv.Multiplier;
        }

        if (!stopping_charging_session) {
            // Charging should only be allowed to run for a specific amount of time. Once the elapsed
            // duration has occurred, gracefully shutdown the charging session to avoid failing the test.
            if (ev_exceeded_scheduled_pmax or TARGET_CHARGING_DURATION < charging_elapsed_time) {
                dlog(DLOG_LEVEL_INFO, "Gracefully stopping charging session");
                stopping_charging_session = true;
                stop_charging(conn->ctx, stopping_charging_session);
            }
        }
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if ((res->DC_EVSEStatus.EVSEStatusCode != last_evse_status_code) or
            (res->DC_EVSEStatus.EVSENotification != last_evse_notification)) {
            last_evse_status_code = res->DC_EVSEStatus.EVSEStatusCode;
            last_evse_notification = res->DC_EVSEStatus.EVSENotification;
            report_response = true;
        }

        if (report_response) {
            report_current_demand_res(conn);
        }
    }

    return next_event;
}

v2g_event DinSmartChargeScheduleTest::handle_din_session_stop(v2g_connection* conn) {
    const auto res = &conn->exi_out.dinEXIDocument->V2G_Message.Body.SessionStopRes;

    report_din_request(conn, V2G_SESSION_STOP_MSG);

    const auto next_event = DinTest::handle_din_session_stop(conn);

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if (res->ResponseCode < din_responseCodeType_FAILED) {
            gracefully_stopped_session = true;
        }
        report_din_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

//=============================================
//             Utility Functions
//=============================================

short DinSmartChargeScheduleTest::get_maximum_power_limit(const din_ChargeParameterDiscoveryReqType* req,
                                                          const din_ChargeParameterDiscoveryResType* res) {
    short maximum_power_limit = SHRT_MAX;

    // EV Maximum Power Limit
    if (req->DC_EVChargeParameter_isUsed and req->DC_EVChargeParameter.EVMaximumPowerLimit_isUsed) {
        const auto ev_maximum_power_limit = calc_physical_value(
            req->DC_EVChargeParameter.EVMaximumPowerLimit.Value,
            req->DC_EVChargeParameter.EVMaximumPowerLimit.Multiplier);
        if (ev_maximum_power_limit < maximum_power_limit)
            maximum_power_limit = static_cast<short>(ev_maximum_power_limit);
    }

    // EVSE Maximum Power Limit
    if (res->SAScheduleList_isUsed and res->SAScheduleList.SAScheduleTuple.arrayLen > 0) {
        if (res->SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.arrayLen > 0) {
            const short evse_maximum_power_limit = res->SAScheduleList.SAScheduleTuple.array[0]
                                                       .PMaxSchedule.PMaxScheduleEntry.array[0]
                                                       .PMax;
            if (evse_maximum_power_limit < maximum_power_limit)
                maximum_power_limit = evse_maximum_power_limit;
        }
    }

    return maximum_power_limit;
}

#pragma endregion DIN_70121

#pragma region ISO_15118_2

//=============================================
//             Event Handlers
//=============================================

void Iso2SmartChargeScheduleTest::on_powermeter_event(v2g_connection* conn, const PowermeterEvent& event) {
    bool power_exists = false;
    float power = 0;

    // Is the power directly provided to us?
    if (event.powermeter.power_W) {
        power = event.powermeter.power_W->total;
        power_exists = true;
    }
    // We cannot accurately determine the power...
    else {
        dlog(DLOG_LEVEL_WARNING, "Cannot determine power from PowerMeter reading!");
    }

    if (power_exists) {
        set_power_reading(power);
    }
}

//=============================================
//             Report Builders
//=============================================

void Iso2SmartChargeScheduleTest::report_charge_parameter_req(const v2g_connection* conn,
                                                              const std::chrono::system_clock::time_point tp) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.ChargeParameterDiscoveryReq;
    std::vector<types::test_report::MessageField> message_fields;

    if (req->AC_EVChargeParameter_isUsed) {
        const auto max_current = iso2_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMaxCurrent, 1);
        const auto max_voltage = iso2_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMaxVoltage, 1);
        const auto min_current = iso2_PhysicalValueType_to_string(req->AC_EVChargeParameter.EVMinCurrent, 1);
        message_fields.push_back({"EVMaxCurrent", max_current});
        message_fields.push_back({"EVMaxVoltage", max_voltage});
        message_fields.push_back({"EVMinCurrent", min_current});
    }

    if (req->DC_EVChargeParameter_isUsed) {
        const auto max_current = iso2_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumCurrentLimit, 1);
        const auto max_voltage = iso2_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumVoltageLimit, 1);
        message_fields.push_back({"EVMaximumCurrentLimit", max_current});
        message_fields.push_back({"EVMaximumVoltageLimit", max_voltage});
        if (req->DC_EVChargeParameter.EVMaximumPowerLimit_isUsed) {
            const auto max_power = iso2_PhysicalValueType_to_string(req->DC_EVChargeParameter.EVMaximumPowerLimit, 1);
            message_fields.push_back({"EVMaximumPowerLimit", max_power});
        }
    }

    report_iso2_request(conn, V2G_CHARGE_PARAMETER_DISCOVERY_MSG, {
        .message_fields = message_fields,
        .metadata { timepoint_to_iso8601_str(tp) },
    });
}

void Iso2SmartChargeScheduleTest::report_charge_parameter_res(const v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.ChargeParameterDiscoveryRes;
    std::vector<types::test_report::MessageField> message_fields;

    const auto evse_processing_str = iso2_EVSEProcessingType_to_string(res->EVSEProcessing);
    message_fields.push_back({"EVSEProcessing", evse_processing_str});

    if (res->SAScheduleList_isUsed) {
        for (int schedule_index = 0; schedule_index < res->SAScheduleList.SAScheduleTuple.arrayLen; ++schedule_index) {
            const auto schedule = &res->SAScheduleList.SAScheduleTuple.array[schedule_index];

            // SAScheduleTupleID
            const auto tuple_id_field_name = fmt::format("SAScheduleList.SAScheduleTuple[{}].SAScheduleTupleID",
                schedule_index);
            const auto tuple_id_field_value = std::to_string(schedule->SAScheduleTupleID);
            message_fields.push_back({tuple_id_field_name, tuple_id_field_value});

            for (int entry_index = 0; entry_index < schedule->PMaxSchedule.PMaxScheduleEntry.arrayLen; ++entry_index) {
                const auto entry = &schedule->PMaxSchedule.PMaxScheduleEntry.array[entry_index];
                const auto entry_field_prefix = fmt::format(
                    "SAScheduleList.SAScheduleTuple[{}].PMaxSchedule.PMaxScheduleEntry[{}]",
                    schedule_index, entry_index);

                // RelativeTimeInterval
                if (entry->RelativeTimeInterval_isUsed) {
                    // Start
                    const auto start_field_name = entry_field_prefix + ".RelativeTimeInterval.start";
                    const auto start_field_value = std::to_string(entry->RelativeTimeInterval.start);
                    message_fields.push_back({start_field_name, start_field_value});

                    // Duration
                    if (entry->RelativeTimeInterval.duration_isUsed) {
                        const auto duration_field_name = entry_field_prefix + ".RelativeTimeInterval.duration";
                        const auto duration_field_value = std::to_string(entry->RelativeTimeInterval.duration);
                        message_fields.push_back({duration_field_name, duration_field_value});
                    }
                }

                // PMax
                const auto pmax_field_name = entry_field_prefix + ".PMax";
                const auto pmax_field_value = iso2_PhysicalValueType_to_string(entry->PMax, 1);
                message_fields.push_back({pmax_field_name, pmax_field_value});
            }
        }
    }

    if (res->AC_EVSEChargeParameter_isUsed) {
        const auto max_current = iso2_PhysicalValueType_to_string(res->AC_EVSEChargeParameter.EVSEMaxCurrent, 1);
        const auto nominal_voltage = iso2_PhysicalValueType_to_string(res->AC_EVSEChargeParameter.EVSENominalVoltage, 1);
        message_fields.push_back({"EVSEMaxCurrent", max_current});
        message_fields.push_back({"EVSENominalVoltage", nominal_voltage});
    }

    if (res->DC_EVSEChargeParameter_isUsed) {
        const auto max_current = iso2_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumCurrentLimit, 1);
        const auto max_voltage = iso2_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumVoltageLimit, 1);
        const auto max_power = iso2_PhysicalValueType_to_string(res->DC_EVSEChargeParameter.EVSEMaximumPowerLimit, 1);
        message_fields.push_back({"EVSEMaximumCurrentLimit", max_current});
        message_fields.push_back({"EVSEMaximumVoltageLimit", max_voltage});
        message_fields.push_back({"EVSEMaximumPowerLimit", max_power});
    }

    report_iso2_response(conn, V2G_CHARGE_PARAMETER_DISCOVERY_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
    });
}

void Iso2SmartChargeScheduleTest::report_current_demand_req(const v2g_connection* conn) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.CurrentDemandReq;
    std::vector<types::test_report::MessageField> message_fields;

    if (not req->DC_EVStatus.EVReady) {
        message_fields.push_back({"EVReady", "false"});
    }

    if (req->DC_EVStatus.EVErrorCode != iso2_DC_EVErrorCodeType_NO_ERROR) {
        const auto error_code = iso2_DC_EVErrorCodeType_to_string(req->DC_EVStatus.EVErrorCode);
        message_fields.push_back({"DC_EVErrorCode", error_code});
    }

    const auto target_current = iso2_PhysicalValueType_to_string(req->EVTargetCurrent, 1);
    const auto target_voltage = iso2_PhysicalValueType_to_string(req->EVTargetVoltage, 1);
    const auto target_current_value = calc_physical_value(req->EVTargetCurrent.Value, req->EVTargetCurrent.Multiplier);
    const auto target_voltage_value = calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier);
    const auto target_power_value = target_current_value * target_voltage_value;
    const auto target_power = double_to_rounded_string(target_power_value, 1) + "W";

    message_fields.push_back({"EVTargetCurrent", target_current});
    message_fields.push_back({"EVTargetVoltage", target_voltage});
    message_fields.push_back({"EVTargetPower", target_power});

    report_iso2_request(conn, V2G_CURRENT_DEMAND_MSG, {
        .message_fields = message_fields,
    });
}

void Iso2SmartChargeScheduleTest::report_current_demand_res(const v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.CurrentDemandRes;
    std::vector<types::test_report::MessageField> message_fields;

    if (res->DC_EVSEStatus.EVSEStatusCode != iso2_DC_EVSEStatusCodeType_EVSE_Ready) {
        const auto status_code = iso2_DC_EVSEStatusCodeType_to_string(res->DC_EVSEStatus.EVSEStatusCode);
        message_fields.push_back({"DC_EVSEStatusCode", status_code});
    }

    if (res->DC_EVSEStatus.EVSENotification != iso2_EVSENotificationType_None) {
        const auto notification = iso2_EVSENotificationType_to_string(res->DC_EVSEStatus.EVSENotification);
        message_fields.push_back({"EVSENotification", notification});
    }

    const auto present_current = iso2_PhysicalValueType_to_string(res->EVSEPresentCurrent, 1);
    const auto present_voltage = iso2_PhysicalValueType_to_string(res->EVSEPresentVoltage, 1);
    const auto present_current_value = calc_physical_value(res->EVSEPresentCurrent.Value, res->EVSEPresentCurrent.Multiplier);
    const auto present_voltage_value = calc_physical_value(res->EVSEPresentVoltage.Value, res->EVSEPresentVoltage.Multiplier);
    const auto present_power_value = present_current_value * present_voltage_value;
    const auto present_power = double_to_rounded_string(present_power_value, 1) + "W";

    message_fields.push_back({"EVSEPresentCurrent", present_current});
    message_fields.push_back({"EVSEPresentVoltage", present_voltage});
    message_fields.push_back({"EVSEPresentPower", present_power});

    report_iso2_response(conn, V2G_CURRENT_DEMAND_MSG, {
        .response_code = res->ResponseCode,
        .message_fields = message_fields,
    });
}

//=============================================
//             Request Handlers
//=============================================

v2g_event Iso2SmartChargeScheduleTest::handle_iso_charge_parameter_discovery(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::seconds;

    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.ChargeParameterDiscoveryReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.ChargeParameterDiscoveryRes;

    // Note the current time the request arrived so we can accurately report the request if the
    // response has an EVSEProcessing value of Finished
    const auto req_arrival_time = std::chrono::system_clock::now();

    auto next_event = Iso2Test::handle_iso_charge_parameter_discovery(conn);

    // Only overwrite the PMax schedule of the last ChargeParameterDiscoveryRes if no other problems occurred
    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {
        if (res->EVSEProcessing == iso2_EVSEProcessingType_Finished) {

            const auto schedule = &res->SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule;
            const auto maximum_power_limit = get_maximum_power_limit(req, res);

            // Calculate the maximum power allowed for each schedule entry
            if (set_maximum_scheduled_power(conn, maximum_power_limit) != 0) {
                // If there were problems creating a power schedule, we cannot proceed.
                dlog(DLOG_LEVEL_INFO, "Failed to create a PMax schedule for the test.");
                res->ResponseCode = iso2_responseCodeType_FAILED;
                next_event = Iso2Test::iso_validate_response_code(&res->ResponseCode, conn);
            }
            // Populate the schedule entries
            else {
                // Define the first schedule entry
                auto schedule_entry = &schedule->PMaxScheduleEntry.array[0];
                init_iso2_PMaxScheduleEntryType(schedule_entry);
                init_iso2_RelativeTimeIntervalType(&schedule_entry->RelativeTimeInterval);
                populate_physical_value(&schedule_entry->PMax, maximum_scheduled_power_0, iso2_unitSymbolType_W);
                schedule_entry->RelativeTimeInterval_isUsed = 1u;
                schedule_entry->RelativeTimeInterval.start = duration_cast<seconds>(SCHEDULE_ENTRY_START_0).count();

                // Define the second schedule entry
                schedule_entry = &schedule->PMaxScheduleEntry.array[1];
                init_iso2_PMaxScheduleEntryType(schedule_entry);
                init_iso2_RelativeTimeIntervalType(&schedule_entry->RelativeTimeInterval);
                populate_physical_value(&schedule_entry->PMax, maximum_scheduled_power_1, iso2_unitSymbolType_W);
                schedule_entry->RelativeTimeInterval_isUsed = 1u;
                schedule_entry->RelativeTimeInterval.start = duration_cast<seconds>(SCHEDULE_ENTRY_START_1).count();

                // Define the third schedule entry
                schedule_entry = &schedule->PMaxScheduleEntry.array[2];
                init_iso2_PMaxScheduleEntryType(schedule_entry);
                init_iso2_RelativeTimeIntervalType(&schedule_entry->RelativeTimeInterval);
                populate_physical_value(&schedule_entry->PMax, maximum_scheduled_power_2, iso2_unitSymbolType_W);
                schedule_entry->RelativeTimeInterval_isUsed = 1u;
                schedule_entry->RelativeTimeInterval.duration_isUsed = 1u;
                schedule_entry->RelativeTimeInterval.duration = duration_cast<seconds>(SCHEDULE_ENTRY_DURATION).count();
                schedule_entry->RelativeTimeInterval.start = duration_cast<seconds>(SCHEDULE_ENTRY_START_2).count();

                // The schedule now contains 3 schedule entries
                schedule->PMaxScheduleEntry.arrayLen = 3;

                dlog(DLOG_LEVEL_INFO, "EVSE Max Power: %dW, Schedule: [%dW, %dW, %dW]",
                     static_cast<int>(maximum_power_limit),
                     static_cast<int>(maximum_scheduled_power_0),
                     static_cast<int>(maximum_scheduled_power_1),
                     static_cast<int>(maximum_scheduled_power_2));

                // This additionally indicates that the PMax schedule was sent
                schedule_start_time = getmonotonictime();
            }
        }
    }

    // Include the final ChargeParameterDiscovery Req/Res in the test report
    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if (res->EVSEProcessing == iso2_EVSEProcessingType_Finished) {
            report_charge_parameter_req(conn, req_arrival_time);
            report_charge_parameter_res(conn);
        }
    }

    return next_event;
}

v2g_event Iso2SmartChargeScheduleTest::handle_iso_power_delivery(v2g_connection* conn) {
    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.PowerDeliveryReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.PowerDeliveryRes;

    report_iso2_request(conn, V2G_POWER_DELIVERY_MSG, {
        .message_fields = {
            {"ChargeProgress", iso2_chargeProgressType_to_string(req->ChargeProgress)},
        }
    });

    const auto next_event = Iso2Test::handle_iso_power_delivery(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {
        // Note the time the EV requests to start charging so we know how long charging has
        // been running during current demand.
        if (req->ChargeProgress == iso2_chargeProgressType_Start and charging_start_time == 0) {
            charging_start_time = getmonotonictime();
        }
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        report_iso2_response(conn, V2G_POWER_DELIVERY_MSG, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

v2g_event Iso2SmartChargeScheduleTest::handle_iso_charging_status(v2g_connection* conn) {
    using std::chrono::milliseconds;

    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.ChargingStatusRes;
    const auto next_event = Iso2Test::handle_iso_charging_status(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {
        const auto now = getmonotonictime();
        const auto charging_elapsed_time = milliseconds(now - charging_start_time);
        const auto schedule_elapsed_time = milliseconds(now - schedule_start_time);
        const auto maybe_present_power = get_and_clear_power_reading();

        if (maybe_present_power.has_value()) {
            const auto present_power = maybe_present_power.value();
            const auto grace_period = milliseconds(V2G_TEST_SCHEDULE_ENTRY_GRACE_PERIOD);

            short last_scheduled_pmax = -1; // previous pmax
            short this_scheduled_pmax = -1; // current pmax
            short next_scheduled_pmax = -1; // upcoming pmax
            std::chrono::seconds last_schedule_start;
            std::chrono::seconds next_schedule_start;

            // In first scheduled time interval?
            if (schedule_elapsed_time < SCHEDULE_ENTRY_START_1) {
                this_scheduled_pmax = maximum_scheduled_power_0;
                next_schedule_start = SCHEDULE_ENTRY_START_1;
                next_scheduled_pmax = maximum_scheduled_power_1;
            }
            // In second scheduled time interval?
            else if (schedule_elapsed_time < SCHEDULE_ENTRY_START_2) {
                last_schedule_start = SCHEDULE_ENTRY_START_0;
                last_scheduled_pmax = maximum_scheduled_power_0;
                this_scheduled_pmax = maximum_scheduled_power_1;
                next_schedule_start = SCHEDULE_ENTRY_START_2;
                next_scheduled_pmax = maximum_scheduled_power_2;
            }
            // In third scheduled time interval?
            else if (schedule_elapsed_time <= SCHEDULE_ENTRY_START_2 + SCHEDULE_ENTRY_DURATION) {
                last_schedule_start = SCHEDULE_ENTRY_START_1;
                last_scheduled_pmax = maximum_scheduled_power_1;
                this_scheduled_pmax = maximum_scheduled_power_2;
            }

            const auto fmt_charging_elapsed = format_duration(charging_elapsed_time);
            const auto fmt_schedule_elapsed = format_duration(schedule_elapsed_time);
            dlog(DLOG_LEVEL_INFO, "Charging time: %s, Schedule time: %s, Present power: %.1fW, Scheduled PMax: %dW",
                 fmt_charging_elapsed.c_str(), fmt_schedule_elapsed.c_str(), present_power,
                 static_cast<int>(this_scheduled_pmax));

            constexpr int tolerance = 50;

            // Did the EV exceed the PMax of the current scheduled time interval?
            if ((this_scheduled_pmax == 0 and present_power > 120) or (this_scheduled_pmax > 1 and present_power > this_scheduled_pmax + tolerance)) {
            // if (present_power > this_scheduled_pmax) {
                // Is the EV within the grace period of the last scheduled time interval?
                if (present_power <= (last_scheduled_pmax + tolerance) and
                    schedule_elapsed_time <= last_schedule_start + SCHEDULE_ENTRY_DURATION + grace_period) {
                    // We are within the grace period of the last PMax Schedule Entry
                    log_grace_period_last_interval();
                }
                // Is the EV within the grace period of the next scheduled time interval?
                else if (present_power <= (next_scheduled_pmax + tolerance) and
                         schedule_elapsed_time >= next_schedule_start - grace_period) {
                    // We are within the grace period of the next PMax Schedule Entry
                    log_grace_period_next_interval();
                }
                // Handle the EV exceeding the PMax of all applicable scheduled time intervals
                else if (!ev_exceeded_scheduled_pmax) {
                    ev_exceeded_scheduled_pmax = true;
                    const auto max_power = this_scheduled_pmax < 0 ? 0 : this_scheduled_pmax;
                    const auto error_message = ac_scheduled_power_exceeded_error(present_power, max_power);
                    conn->ctx->test_data.errors.push_back(error_message);
                    dlog(DLOG_LEVEL_INFO, error_message.c_str());
                }
            }

            // Check if the EV is drawing non-zero power for test pass-criteria validation
            if (!ev_requested_nonzero_power and present_power > 120) {
                ev_requested_nonzero_power = true;
            }

            // Update the EVSEMaxCurrent to reflect the active PMax constraint
            if (res->EVSEMaxCurrent_isUsed) {
                // TODO: Update EVSEMaxCurrent
            }
        }

        if (!stopping_charging_session) {
            // Charging should only be allowed to run for a specific amount of time. Once the elapsed
            // duration has occurred, gracefully shutdown the charging session to avoid failing the test.
            if (ev_exceeded_scheduled_pmax or TARGET_CHARGING_DURATION < charging_elapsed_time) {
                dlog(DLOG_LEVEL_INFO, "Gracefully stopping charging session");
                stopping_charging_session = true;
                stop_charging(conn->ctx, stopping_charging_session);
            }
        }
    }

    return next_event;
}

v2g_event Iso2SmartChargeScheduleTest::handle_iso_current_demand(v2g_connection* conn) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    const auto req = &conn->exi_in.iso2EXIDocument->V2G_Message.Body.CurrentDemandReq;
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.CurrentDemandRes;

    const auto ev_target_current = calc_physical_value(req->EVTargetCurrent.Value, req->EVTargetCurrent.Multiplier);
    const auto ev_target_voltage = calc_physical_value(req->EVTargetVoltage.Value, req->EVTargetVoltage.Multiplier);
    const auto ev_target_power = ev_target_current * ev_target_voltage;
    bool report_response = false;

    if ((std::abs(ev_target_power - last_target_power) > 1) or
        (req->DC_EVStatus.EVErrorCode != last_ev_error_code) or
        (req->DC_EVStatus.EVReady != last_ev_ready)) {

        last_target_power = ev_target_power;
        last_ev_error_code = req->DC_EVStatus.EVErrorCode;
        last_ev_ready = req->DC_EVStatus.EVReady;
        report_response = true;

        report_current_demand_req(conn);
    }

    const auto next_event = Iso2Test::handle_iso_current_demand(conn);

    if (next_event == V2G_EVENT_NO_EVENT and res->ResponseCode < iso2_responseCodeType_FAILED) {

        const auto now = getmonotonictime();
        const auto charging_elapsed_time = milliseconds(now - charging_start_time);
        const auto schedule_elapsed_time = milliseconds(now - schedule_start_time);
        const auto grace_period = milliseconds(V2G_TEST_SCHEDULE_ENTRY_GRACE_PERIOD);

        short last_scheduled_pmax = -1; // previous pmax
        short this_scheduled_pmax = -1; // current pmax
        short next_scheduled_pmax = -1; // upcoming pmax
        std::chrono::seconds last_schedule_start;
        std::chrono::seconds next_schedule_start;

        // In first scheduled time interval?
        if (schedule_elapsed_time < SCHEDULE_ENTRY_START_1) {
            this_scheduled_pmax = maximum_scheduled_power_0;
            next_schedule_start = SCHEDULE_ENTRY_START_1;
            next_scheduled_pmax = maximum_scheduled_power_1;
        }
        // In second scheduled time interval?
        else if (schedule_elapsed_time < SCHEDULE_ENTRY_START_2) {
            last_schedule_start = SCHEDULE_ENTRY_START_0;
            last_scheduled_pmax = maximum_scheduled_power_0;
            this_scheduled_pmax = maximum_scheduled_power_1;
            next_schedule_start = SCHEDULE_ENTRY_START_2;
            next_scheduled_pmax = maximum_scheduled_power_2;
        }
        // In third scheduled time interval?
        else if (schedule_elapsed_time <= SCHEDULE_ENTRY_START_2 + SCHEDULE_ENTRY_DURATION) {
            last_schedule_start = SCHEDULE_ENTRY_START_1;
            last_scheduled_pmax = maximum_scheduled_power_1;
            this_scheduled_pmax = maximum_scheduled_power_2;
        }

        // Did the EV exceed the PMax of the current scheduled time interval?
        if (ev_target_power > this_scheduled_pmax) {
            // Is the EV within the grace period of the last scheduled time interval?
            if (ev_target_power <= last_scheduled_pmax and
                schedule_elapsed_time <= last_schedule_start + SCHEDULE_ENTRY_DURATION + grace_period) {
                // We are within the grace period of the last PMax Schedule Entry
                log_grace_period_last_interval();
            }
            // Is the EV within the grace period of the next scheduled time interval?
            else if (ev_target_power <= next_scheduled_pmax and
                     schedule_elapsed_time >= next_schedule_start - grace_period) {
                // We are within the grace period of the next PMax Schedule Entry
                log_grace_period_next_interval();
            }
            // Handle the EV exceeding the PMax of all applicable scheduled time intervals
            else if (!ev_exceeded_scheduled_pmax) {
                ev_exceeded_scheduled_pmax = true;
                const auto max_power = this_scheduled_pmax < 0 ? 0 : this_scheduled_pmax;
                const auto error_message = dc_scheduled_power_exceeded_error(ev_target_power, max_power);
                conn->ctx->test_data.errors.push_back(error_message);
                dlog(DLOG_LEVEL_INFO, error_message.c_str());
            }
        }

        // Check if the EV requested non-zero power for test pass-criteria validation
        if (!ev_requested_nonzero_power and ev_target_power > 1) {
            ev_requested_nonzero_power = true;
        }

        // Update the EVSE Maximum Power Limit in the response to reflect the PMax value
        if (res->EVSEMaximumPowerLimit_isUsed) {
            const auto max_power = this_scheduled_pmax < 0 ? 0 : this_scheduled_pmax;
            populate_physical_value(&res->EVSEMaximumPowerLimit, max_power, iso2_unitSymbolType_W);
        }

        if (!stopping_charging_session) {
            // Charging should only be allowed to run for a specific amount of time. Once the elapsed
            // duration has occurred, gracefully shutdown the charging session to avoid failing the test.
            if (ev_exceeded_scheduled_pmax or TARGET_CHARGING_DURATION < charging_elapsed_time) {
                dlog(DLOG_LEVEL_INFO, "Gracefully stopping charging session");
                stopping_charging_session = true;
                stop_charging(conn->ctx, stopping_charging_session);
            }
        }
    }

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if ((res->DC_EVSEStatus.EVSEStatusCode != last_evse_status_code) or
            (res->DC_EVSEStatus.EVSENotification != last_evse_notification)) {
            last_evse_status_code = res->DC_EVSEStatus.EVSEStatusCode;
            last_evse_notification = res->DC_EVSEStatus.EVSENotification;
            report_response = true;
        }

        if (report_response) {
            report_current_demand_res(conn);
        }
    }

    return next_event;
}

v2g_event Iso2SmartChargeScheduleTest::handle_iso_session_stop(v2g_connection* conn) {
    const auto res = &conn->exi_out.iso2EXIDocument->V2G_Message.Body.SessionStopRes;

    report_iso2_request(conn, V2G_SESSION_STOP_MSG);

    const auto next_event = Iso2Test::handle_iso_session_stop(conn);

    if (next_event == V2G_EVENT_NO_EVENT or next_event == V2G_EVENT_SEND_AND_TERMINATE) {
        if (res->ResponseCode < iso2_responseCodeType_FAILED) {
            gracefully_stopped_session = true;
        }
        report_iso2_response(conn, V2G_SESSION_STOP_MSG, {
            .response_code = res->ResponseCode,
        });
    }

    return next_event;
}

//=============================================
//             Utility Functions
//=============================================

short Iso2SmartChargeScheduleTest::get_maximum_power_limit(const iso2_ChargeParameterDiscoveryReqType* req,
                                                           const iso2_ChargeParameterDiscoveryResType* res) {
    short maximum_power_limit = SHRT_MAX;

    // EV Maximum Power Limit (AC)
    if (req->AC_EVChargeParameter_isUsed) {
        const auto ev_maximum_current = calc_physical_value(
            req->AC_EVChargeParameter.EVMaxCurrent.Value,
            req->AC_EVChargeParameter.EVMaxCurrent.Multiplier);
        const auto ev_maximum_voltage = calc_physical_value(
            req->AC_EVChargeParameter.EVMaxVoltage.Value,
            req->AC_EVChargeParameter.EVMaxVoltage.Multiplier);
        const auto ev_maximum_power = ev_maximum_current * ev_maximum_voltage;
        if (ev_maximum_power < maximum_power_limit)
            maximum_power_limit = static_cast<short>(ev_maximum_power);
    }

    // EV Maximum Power Limit (DC)
    if (req->DC_EVChargeParameter_isUsed and req->DC_EVChargeParameter.EVMaximumPowerLimit_isUsed) {
        const auto ev_maximum_power_limit = calc_physical_value(
            req->DC_EVChargeParameter.EVMaximumPowerLimit.Value,
            req->DC_EVChargeParameter.EVMaximumPowerLimit.Multiplier);
        if (ev_maximum_power_limit < maximum_power_limit)
            maximum_power_limit = static_cast<short>(ev_maximum_power_limit);
    }

    // EVSE Maximum Power Limit
    if (res->SAScheduleList_isUsed and res->SAScheduleList.SAScheduleTuple.arrayLen > 0) {
        if (res->SAScheduleList.SAScheduleTuple.array[0].PMaxSchedule.PMaxScheduleEntry.arrayLen > 0) {
            const auto evse_maximum_power_limit_pv = res->SAScheduleList.SAScheduleTuple.array[0]
                                                     .PMaxSchedule.PMaxScheduleEntry.array[0]
                                                     .PMax;
            const auto evse_maximum_power_limit = calc_physical_value(
                evse_maximum_power_limit_pv.Value,
                evse_maximum_power_limit_pv.Multiplier);
            if (evse_maximum_power_limit < maximum_power_limit)
                maximum_power_limit = static_cast<short>(evse_maximum_power_limit);
        }
    }

    return maximum_power_limit;
}

void Iso2SmartChargeScheduleTest::set_power_reading(const float power) {
    std::lock_guard lock(powermeter_mutex);
    power_reading_W = power;
    power_reading_updated = true;
}

std::optional<float> Iso2SmartChargeScheduleTest::get_and_clear_power_reading() {
    std::lock_guard lock(powermeter_mutex);
    if (power_reading_updated) {
        power_reading_updated = false;
        return std::optional(power_reading_W);
    }
    return std::nullopt;
}

#pragma endregion ISO_15118_2

} // namespace testing::chx_smart_charging_scheduling
