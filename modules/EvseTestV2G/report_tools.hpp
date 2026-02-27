// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef REPORT_TOOLS_HPP
#define REPORT_TOOLS_HPP

#include <string>
#include <cbv2g/din/din_msgDefDatatypes.h>
#include <cbv2g/iso_2/iso2_msgDefDatatypes.h>

namespace testing {

std::string double_to_rounded_string(double value, int precision);
const char* din_unitSymbolType_to_string(const din_unitSymbolType& unit_symbol);
const char* iso2_unitSymbolType_to_string(const iso2_unitSymbolType& unit_symbol);

/**
 * @brief Convert a DIN 70121 PhysicalValueType instance to a string.
 * @param pv The PhysicalValueType instance to convert to a string.
 * @param precision The number of decimal places to round the value to.
 * @return A string representing the value and unit.
 */
std::string din_PhysicalValueType_to_string(const din_PhysicalValueType& pv, int precision);

/**
 * @brief Convert a ISO 15118-2 PhysicalValueType instance to a string.
 * @param pv The PhysicalValueType instance to convert to a string.
 * @param precision The number of decimal places to round the value to.
 * @return A string representing the value and unit.
 */
std::string iso2_PhysicalValueType_to_string(const iso2_PhysicalValueType& pv, int precision);

const char* din_DC_EVErrorCodeType_to_string(const din_DC_EVErrorCodeType& ev_error_code);

const char* iso2_DC_EVErrorCodeType_to_string(const iso2_DC_EVErrorCodeType& ev_error_code);

const char* din_DC_EVSEStatusCodeType_to_string(const din_DC_EVSEStatusCodeType& evse_status_code);

const char* iso2_DC_EVSEStatusCodeType_to_string(const iso2_DC_EVSEStatusCodeType& evse_status_code);

const char* din_EVSENotificationType_to_string(const din_EVSENotificationType& notification);

const char* iso2_EVSENotificationType_to_string(const iso2_EVSENotificationType& notification);

const char* din_EVSEProcessingType_to_string(din_EVSEProcessingType evse_processing);

const char* iso2_EVSEProcessingType_to_string(iso2_EVSEProcessingType evse_processing);

const char* din_isolationLevelType_to_string(din_isolationLevelType isolation_level);

const char* iso2_isolationLevelType_to_string(iso2_isolationLevelType isolation_level);

const char* iso2_chargeProgressType_to_string(iso2_chargeProgressType charge_progress);

std::string ac_scheduled_power_exceeded_error(float power, float max_power);
std::string dc_scheduled_power_exceeded_error(float power, float max_power);

} // namespace testing

#endif //REPORT_TOOLS_HPP
