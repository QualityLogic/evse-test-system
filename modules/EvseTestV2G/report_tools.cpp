// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "report_tools.hpp"
#include "fmt/format.h"
#include "tools.hpp"

#include <iomanip>
#include <sstream>

namespace testing {

[[nodiscard]] std::string double_to_rounded_string(const double value, const int precision) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

[[nodiscard]] const char* din_unitSymbolType_to_string(const din_unitSymbolType& unit_symbol) {
    switch (unit_symbol) {
    case din_unitSymbolType_h:
        return "h";
    case din_unitSymbolType_m:
        return "m";
    case din_unitSymbolType_s:
        return "s";
    case din_unitSymbolType_A:
        return "A";
    case din_unitSymbolType_Ah:
        return "Ah";
    case din_unitSymbolType_V:
        return "V";
    case din_unitSymbolType_VA:
        return "VA";
    case din_unitSymbolType_W:
        return "W";
    case din_unitSymbolType_W_s:
        return "W s";
    case din_unitSymbolType_Wh:
        return "Wh";
    default:
        return "";
    }
}

[[nodiscard]] const char* iso2_unitSymbolType_to_string(const iso2_unitSymbolType& unit_symbol) {
    switch (unit_symbol) {
    case iso2_unitSymbolType_h:
        return "h";
    case iso2_unitSymbolType_m:
        return "m";
    case iso2_unitSymbolType_s:
        return "s";
    case iso2_unitSymbolType_A:
        return "A";
    case iso2_unitSymbolType_V:
        return "V";
    case iso2_unitSymbolType_W:
        return "W";
    case iso2_unitSymbolType_Wh:
        return "Wh";
    default:
        return "";
    }
}

[[nodiscard]] std::string din_PhysicalValueType_to_string(const din_PhysicalValueType& pv, const int precision) {
    const auto value = calc_physical_value(pv.Value, pv.Multiplier);
    auto result = double_to_rounded_string(value, precision);
    if (pv.Unit_isUsed)
        result.append(din_unitSymbolType_to_string(pv.Unit));
    return result;
}

[[nodiscard]] std::string iso2_PhysicalValueType_to_string(const iso2_PhysicalValueType& pv, const int precision) {
    const auto value = calc_physical_value(pv.Value, pv.Multiplier);
    auto result = double_to_rounded_string(value, precision);
    result.append(iso2_unitSymbolType_to_string(pv.Unit));
    return result;
}

[[nodiscard]] const char* din_DC_EVErrorCodeType_to_string(const din_DC_EVErrorCodeType& ev_error_code) {
    switch (ev_error_code) {
    case din_DC_EVErrorCodeType_NO_ERROR:
        return "NO_ERROR";
    case din_DC_EVErrorCodeType_FAILED_RESSTemperatureInhibit:
        return "FAILED_RESSTemperatureInhibit";
    case din_DC_EVErrorCodeType_FAILED_EVShiftPosition:
        return "FAILED_EVShiftPosition";
    case din_DC_EVErrorCodeType_FAILED_ChargerConnectorLockFault:
        return "FAILED_ChargerConnectorLockFault";
    case din_DC_EVErrorCodeType_FAILED_EVRESSMalfunction:
        return "FAILED_EVRESSMalfunction";
    case din_DC_EVErrorCodeType_FAILED_ChargingCurrentdifferential:
        return "FAILED_ChargingCurrentdifferential";
    case din_DC_EVErrorCodeType_FAILED_ChargingVoltageOutOfRange:
        return "FAILED_ChargingVoltageOutOfRange";
    case din_DC_EVErrorCodeType_FAILED_ChargingSystemIncompatibility:
        return "FAILED_ChargingSystemIncompatibility";
    case din_DC_EVErrorCodeType_NoData:
        return "NoData";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* iso2_DC_EVErrorCodeType_to_string(const iso2_DC_EVErrorCodeType& ev_error_code) {
    switch (ev_error_code) {
    case iso2_DC_EVErrorCodeType_NO_ERROR:
        return "NO_ERROR";
    case iso2_DC_EVErrorCodeType_FAILED_RESSTemperatureInhibit:
        return "FAILED_RESSTemperatureInhibit";
    case iso2_DC_EVErrorCodeType_FAILED_EVShiftPosition:
        return "FAILED_EVShiftPosition";
    case iso2_DC_EVErrorCodeType_FAILED_ChargerConnectorLockFault:
        return "FAILED_ChargerConnectorLockFault";
    case iso2_DC_EVErrorCodeType_FAILED_EVRESSMalfunction:
        return "FAILED_EVRESSMalfunction";
    case iso2_DC_EVErrorCodeType_FAILED_ChargingCurrentdifferential:
        return "FAILED_ChargingCurrentdifferential";
    case iso2_DC_EVErrorCodeType_FAILED_ChargingVoltageOutOfRange:
        return "FAILED_ChargingVoltageOutOfRange";
    case iso2_DC_EVErrorCodeType_FAILED_ChargingSystemIncompatibility:
        return "FAILED_ChargingSystemIncompatibility";
    case iso2_DC_EVErrorCodeType_NoData:
        return "NoData";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* din_DC_EVSEStatusCodeType_to_string(const din_DC_EVSEStatusCodeType& evse_status_code) {
    switch (evse_status_code) {
    case din_DC_EVSEStatusCodeType_EVSE_NotReady:
        return "EVSE_NotReady";
    case din_DC_EVSEStatusCodeType_EVSE_Ready:
        return "EVSE_Ready";
    case din_DC_EVSEStatusCodeType_EVSE_Shutdown:
        return "EVSE_Shutdown";
    case din_DC_EVSEStatusCodeType_EVSE_UtilityInterruptEvent:
        return "EVSE_UtilityInterruptEvent";
    case din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive:
        return "EVSE_IsolationMonitoringActive";
    case din_DC_EVSEStatusCodeType_EVSE_EmergencyShutdown:
        return "EVSE_EmergencyShutdown";
    case din_DC_EVSEStatusCodeType_EVSE_Malfunction:
        return "EVSE_Malfunction";
    case din_DC_EVSEStatusCodeType_Reserved_8:
    case din_DC_EVSEStatusCodeType_Reserved_9:
    case din_DC_EVSEStatusCodeType_Reserved_A:
    case din_DC_EVSEStatusCodeType_Reserved_B:
    case din_DC_EVSEStatusCodeType_Reserved_C:
        return "Reserved";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* iso2_DC_EVSEStatusCodeType_to_string(const iso2_DC_EVSEStatusCodeType& evse_status_code) {
    switch (evse_status_code) {
    case iso2_DC_EVSEStatusCodeType_EVSE_NotReady:
        return "EVSE_NotReady";
    case iso2_DC_EVSEStatusCodeType_EVSE_Ready:
        return "EVSE_Ready";
    case iso2_DC_EVSEStatusCodeType_EVSE_Shutdown:
        return "EVSE_Shutdown";
    case iso2_DC_EVSEStatusCodeType_EVSE_UtilityInterruptEvent:
        return "EVSE_UtilityInterruptEvent";
    case iso2_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive:
        return "EVSE_IsolationMonitoringActive";
    case iso2_DC_EVSEStatusCodeType_EVSE_EmergencyShutdown:
        return "EVSE_EmergencyShutdown";
    case iso2_DC_EVSEStatusCodeType_EVSE_Malfunction:
        return "EVSE_Malfunction";
    case iso2_DC_EVSEStatusCodeType_Reserved_8:
    case iso2_DC_EVSEStatusCodeType_Reserved_9:
    case iso2_DC_EVSEStatusCodeType_Reserved_A:
    case iso2_DC_EVSEStatusCodeType_Reserved_B:
    case iso2_DC_EVSEStatusCodeType_Reserved_C:
        return "Reserved";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* din_EVSENotificationType_to_string(const din_EVSENotificationType& notification) {
    switch (notification) {
    case din_EVSENotificationType_None:
        return "None";
    case din_EVSENotificationType_StopCharging:
        return "StopCharging";
    case din_EVSENotificationType_ReNegotiation:
        return "ReNegotiation";
    default:
        return "Unknown";
    }
}

[[nodiscard]] const char* iso2_EVSENotificationType_to_string(const iso2_EVSENotificationType& notification) {
    switch (notification) {
    case iso2_EVSENotificationType_None:
        return "None";
    case iso2_EVSENotificationType_StopCharging:
        return "StopCharging";
    case iso2_EVSENotificationType_ReNegotiation:
        return "ReNegotiation";
    default:
        return "Unknown";
    }
}

const char* din_EVSEProcessingType_to_string(const din_EVSEProcessingType evse_processing) {
    switch (evse_processing) {
    case din_EVSEProcessingType_Finished:
        return "Finished";
    case din_EVSEProcessingType_Ongoing:
        return "Ongoing";
    default:
        return "Unknown";
    }
}

const char* iso2_EVSEProcessingType_to_string(const iso2_EVSEProcessingType evse_processing) {
    switch (evse_processing) {
    case iso2_EVSEProcessingType_Finished:
        return "Finished";
    case iso2_EVSEProcessingType_Ongoing:
        return "Ongoing";
    case iso2_EVSEProcessingType_Ongoing_WaitingForCustomerInteraction:
        return "Ongoing_WaitingForCustomerInteraction";
    default:
        return "Unknown";
    }
}

const char* din_isolationLevelType_to_string(const din_isolationLevelType isolation_level) {
    switch (isolation_level) {
    case din_isolationLevelType_Invalid:
        return "Invalid";
    case din_isolationLevelType_Valid:
        return "Valid";
    case din_isolationLevelType_Warning:
        return "Warning";
    case din_isolationLevelType_Fault:
        return "Fault";
    default:
        return "Unknown";
    }
}

const char* iso2_isolationLevelType_to_string(const iso2_isolationLevelType isolation_level) {
    switch (isolation_level) {
    case iso2_isolationLevelType_Invalid:
        return "Invalid";
    case iso2_isolationLevelType_Valid:
        return "Valid";
    case iso2_isolationLevelType_Warning:
        return "Warning";
    case iso2_isolationLevelType_Fault:
        return "Fault";
    case iso2_isolationLevelType_No_IMD:
        return "No_IMD";
    default:
        return "Unknown";
    }
}

const char* iso2_chargeProgressType_to_string(const iso2_chargeProgressType charge_progress) {
    switch (charge_progress) {
    case iso2_chargeProgressType_Start:
        return "Start";
    case iso2_chargeProgressType_Stop:
        return "Stop";
    case iso2_chargeProgressType_Renegotiate:
        return "Renegotiate";
    default:
        return "Unknown";
    }
}

std::string ac_scheduled_power_exceeded_error(const float power, const float max_power) {
    const auto power_str = double_to_rounded_string(power, 1);
    const auto max_power_str = double_to_rounded_string(max_power, 1);
    return fmt::format("Power draw ({} W) exceeds scheduled PMax ({} W)",
                       power_str.c_str(), max_power_str.c_str());
}

std::string dc_scheduled_power_exceeded_error(const float power, const float max_power) {
    const auto power_str = double_to_rounded_string(power, 1);
    const auto max_power_str = double_to_rounded_string(max_power, 1);
    return fmt::format("EV Target Power ({} W) exceeds scheduled PMax ({} W)",
                       power_str.c_str(), max_power_str.c_str());
}

} // namespace testing
