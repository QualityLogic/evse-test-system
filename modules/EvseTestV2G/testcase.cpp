// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "testcase.hpp"

#include "tools.hpp"
#include "log.hpp"
#include "v2g.hpp"

namespace testing {

static const char* din_response_code_names[] = {
    "OK",
    "OK_NewSessionEstablished",
    "OK_OldSessionJoined",
    "OK_CertificateExpiresSoon",
    "FAILED",
    "FAILED_SequenceError",
    "FAILED_ServiceIDInvalid",
    "FAILED_UnknownSession",
    "FAILED_ServiceSelectionInvalid",
    "FAILED_PaymentSelectionInvalid",
    "FAILED_CertificateExpired",
    "FAILED_SignatureError",
    "FAILED_NoCertificateAvailable",
    "FAILED_CertChainError",
    "FAILED_ChallengeInvalid",
    "FAILED_ContractCanceled",
    "FAILED_WrongChargeParameter",
    "FAILED_PowerDeliveryNotApplied",
    "FAILED_TariffSelectionInvalid",
    "FAILED_ChargingProfileInvalid",
    "FAILED_EVSEPresentVoltageToLow",
    "FAILED_MeteringSignatureNotValid",
    "FAILED_WrongEnergyTransferType",
};

static const char* iso2_response_code_names[] = {
    "OK",
    "OK_NewSessionEstablished",
    "OK_OldSessionJoined",
    "OK_CertificateExpiresSoon",
    "FAILED",
    "FAILED_SequenceError",
    "FAILED_ServiceIDInvalid",
    "FAILED_UnknownSession",
    "FAILED_ServiceSelectionInvalid",
    "FAILED_PaymentSelectionInvalid",
    "FAILED_CertificateExpired",
    "FAILED_SignatureError",
    "FAILED_NoCertificateAvailable",
    "FAILED_CertChainError",
    "FAILED_ChallengeInvalid",
    "FAILED_ContractCanceled",
    "FAILED_WrongChargeParameter",
    "FAILED_PowerDeliveryNotApplied",
    "FAILED_TariffSelectionInvalid",
    "FAILED_ChargingProfileInvalid",
    "FAILED_MeteringSignatureNotValid",
    "FAILED_NoChargeServiceSelected",
    "FAILED_WrongEnergyTransferMode",
    "FAILED_ContactorError",
    "FAILED_CertificateNotAllowedAtThisEVSE",
    "FAILED_CertificateRevoked",
};

void Test::stop_charging(v2g_context* ctx, bool& stop) {
    // FIXME we need to use locks on v2g-ctx in all commands as they are running in different threads

    if (stop) {
        dlog(DLOG_LEVEL_DEBUG, "Session :: STOP CHARGING");

        // spawn new thread to not block command handler
        std::thread([ctx, stop] {
            // try to gracefully shutdown charging session
            ctx->evse_v2g_data.evse_notification = iso2_EVSENotificationType_StopCharging;
            memset(ctx->evse_v2g_data.evse_status_code, iso2_DC_EVSEStatusCodeType_EVSE_Shutdown,
                   sizeof(ctx->evse_v2g_data.evse_status_code));

            int i;
            bool timeout_reached = true;
            // allow 10 seconds for graceful shutdown
            for (i = 0; i < 10; i++) {
                if (ctx->is_connection_terminated) {
                    timeout_reached = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // If it did not stop within timeout, stop session using FAILED reply
            if (timeout_reached) {
                dlog(DLOG_LEVEL_DEBUG, "Session :: FORCEFULLY STOP CHARGING");
                ctx->stop_hlc = stop;
            }
        }).detach();
    } else {
        ctx->stop_hlc = false;
    }
}

//=============================================
//             Report Builders
//=============================================

/**
 * Build a report to describe a ConnectionClosed event.
 * @param metadata Additional context for the reported event.
 * @return A report describing the ConnectionClosed event.
 */
static types::test_report::Report build_connection_closed_report(const types::test_report::ReportMetadata& metadata) {
    return {
        .report_type = types::test_report::ReportType::ConnectionClosed,
        .metadata = metadata
    };
}

/**
 * Build a report to describe a BspMeasurement event.
 * @param bsp_event The BSP event to report.
 * @param metadata Additional context for the reported event.
 * @return A report describing the BspEvent event.
 */
static types::test_report::Report build_bsp_event_report(const types::board_support_common::BspEvent& bsp_event,
                                                         const types::test_report::ReportMetadata& metadata) {
    return {
        .report_type = types::test_report::ReportType::BspEvent,
        .bsp_event_data = bsp_event,
        .metadata = metadata
    };
}

/**
 * @brief Build a report to describe a BspMeasurement event.
 * @param bsp_measurement The BSP measurement to report.
 * @param metadata Additional context for the reported event.
 * @return A report describing the BspMeasurement event.
 */
static types::test_report::Report build_bsp_measurement_report(
    const types::board_support_common::BspMeasurement& bsp_measurement,
    const types::test_report::ReportMetadata& metadata) {

    return {
        .report_type = types::test_report::ReportType::BspMeasurement,
        .bsp_measurement_data = bsp_measurement,
        .metadata = metadata
    };
}

/**
 * @brief Build a report to describe a V2gMessage event.
 * @param v2g_message_id ID of the V2G message body.
 * @param message_fields A list of V2G message fields.
 * @param metadata Additional context for the reported event.
 * @return A report describing the V2gMessage event.
 */
static types::test_report::Report build_v2g_message_report(
    const types::iso15118::V2gMessageId& v2g_message_id,
    const std::vector<types::test_report::MessageField>& message_fields,
    const types::test_report::ReportMetadata& metadata) {

    return {
        .report_type = types::test_report::ReportType::V2gMessage,
        .v2g_message_data = types::test_report::V2gMessageDetails{
            .message_id = v2g_message_id,
            .message_fields = message_fields
        },
        .metadata = metadata
    };
}

/**
 * @brief Provide default values to missing but required metadata fields.
 *
 * This should be called once by every reporting function.
 *
 * @param metadata The metadata object to prepare.
 */
static void prepare_metadata(types::test_report::ReportMetadata& metadata) {
    // If a timestamp was not explicitly set, use the current time.
    if (not metadata.timestamp) {
        const auto now = std::chrono::system_clock::now();
        metadata.timestamp = timepoint_to_iso8601_str(now);
    }
}

/**
 * @brief Convert a DIN 70121 V2G request message into its matching V2gMessageId.
 * @param message_type The V2G message type to convert.
 * @return The equivalent V2gMessageId for the message type.
 */
static types::iso15118::V2gMessageId get_din_request_id(const V2gMsgTypeId message_type) {
    using types::iso15118::V2gMessageId;

    switch (message_type) {
    case V2G_SUPPORTED_APP_PROTOCOL_MSG: return V2gMessageId::SupportedAppProtocolReq;
    case V2G_SESSION_SETUP_MSG: return V2gMessageId::SessionSetupReq;
    case V2G_SERVICE_DISCOVERY_MSG: return V2gMessageId::ServiceDiscoveryReq;
    case V2G_SERVICE_DETAIL_MSG: return V2gMessageId::ServiceDetailReq;
    case V2G_PAYMENT_SERVICE_SELECTION_MSG: return V2gMessageId::ServicePaymentSelectionReq;
    case V2G_PAYMENT_DETAILS_MSG: return V2gMessageId::PaymentDetailsReq;
    case V2G_AUTHORIZATION_MSG: return V2gMessageId::ContractAuthenticationReq;
    case V2G_CHARGE_PARAMETER_DISCOVERY_MSG: return V2gMessageId::ChargeParameterDiscoveryReq;
    case V2G_METERING_RECEIPT_MSG: return V2gMessageId::MeteringReceiptReq;
    case V2G_CERTIFICATE_UPDATE_MSG: return V2gMessageId::CertificateUpdateReq;
    case V2G_CERTIFICATE_INSTALLATION_MSG: return V2gMessageId::CertificateInstallationReq;
    case V2G_CHARGING_STATUS_MSG: return V2gMessageId::ChargingStatusReq;
    case V2G_CABLE_CHECK_MSG: return V2gMessageId::CableCheckReq;
    case V2G_PRE_CHARGE_MSG: return V2gMessageId::PreChargeReq;
    case V2G_POWER_DELIVERY_MSG: return V2gMessageId::PowerDeliveryReq;
    case V2G_CURRENT_DEMAND_MSG: return V2gMessageId::CurrentDemandReq;
    case V2G_WELDING_DETECTION_MSG: return V2gMessageId::WeldingDetectionReq;
    case V2G_SESSION_STOP_MSG: return V2gMessageId::SessionStopReq;
    case V2G_UNKNOWN_MSG:
    default:
        return V2gMessageId::UnknownMessage;
    }
}

/**
 * @brief Convert a DIN 70121 V2G response message into its matching V2gMessageId.
 * @param message_type The V2G message type to convert.
 * @return The equivalent V2gMessageId for the message type.
 */
static types::iso15118::V2gMessageId get_din_response_id(const V2gMsgTypeId message_type) {
    using types::iso15118::V2gMessageId;

    switch (message_type) {
    case V2G_SUPPORTED_APP_PROTOCOL_MSG: return V2gMessageId::SupportedAppProtocolRes;
    case V2G_SESSION_SETUP_MSG: return V2gMessageId::SessionSetupRes;
    case V2G_SERVICE_DISCOVERY_MSG: return V2gMessageId::ServiceDiscoveryRes;
    case V2G_SERVICE_DETAIL_MSG: return V2gMessageId::ServiceDetailRes;
    case V2G_PAYMENT_SERVICE_SELECTION_MSG: return V2gMessageId::ServicePaymentSelectionRes;
    case V2G_PAYMENT_DETAILS_MSG: return V2gMessageId::PaymentDetailsRes;
    case V2G_AUTHORIZATION_MSG: return V2gMessageId::ContractAuthenticationRes;
    case V2G_CHARGE_PARAMETER_DISCOVERY_MSG: return V2gMessageId::ChargeParameterDiscoveryRes;
    case V2G_METERING_RECEIPT_MSG: return V2gMessageId::MeteringReceiptRes;
    case V2G_CERTIFICATE_UPDATE_MSG: return V2gMessageId::CertificateUpdateRes;
    case V2G_CERTIFICATE_INSTALLATION_MSG: return V2gMessageId::CertificateInstallationRes;
    case V2G_CHARGING_STATUS_MSG: return V2gMessageId::ChargingStatusRes;
    case V2G_CABLE_CHECK_MSG: return V2gMessageId::CableCheckRes;
    case V2G_PRE_CHARGE_MSG: return V2gMessageId::PreChargeRes;
    case V2G_POWER_DELIVERY_MSG: return V2gMessageId::PowerDeliveryRes;
    case V2G_CURRENT_DEMAND_MSG: return V2gMessageId::CurrentDemandRes;
    case V2G_WELDING_DETECTION_MSG: return V2gMessageId::WeldingDetectionRes;
    case V2G_SESSION_STOP_MSG: return V2gMessageId::SessionStopRes;
    case V2G_UNKNOWN_MSG:
    default:
        return V2gMessageId::UnknownMessage;
    }
}

/// \brief Converts the given din_responseCodeType \p response_code to a human-readable string
/// \return a string representation of the din_responseCodeType
static std::string din_response_code_to_string(const din_responseCodeType response_code) {
    return din_response_code_names[response_code];
}

/**
 * @brief Convert an ISO 15118-2 V2G request message into its matching V2gMessageId.
 * @param message_type The V2G message type to convert.
 * @return The equivalent V2gMessageId for the message type.
 */
static types::iso15118::V2gMessageId get_iso2_request_id(const V2gMsgTypeId message_type) {
    using types::iso15118::V2gMessageId;

    switch (message_type) {
    case V2G_SUPPORTED_APP_PROTOCOL_MSG: return V2gMessageId::SupportedAppProtocolReq;
    case V2G_SESSION_SETUP_MSG: return V2gMessageId::SessionSetupReq;
    case V2G_SERVICE_DISCOVERY_MSG: return V2gMessageId::ServiceDiscoveryReq;
    case V2G_SERVICE_DETAIL_MSG: return V2gMessageId::ServiceDetailReq;
    case V2G_PAYMENT_SERVICE_SELECTION_MSG: return V2gMessageId::PaymentServiceSelectionReq;
    case V2G_PAYMENT_DETAILS_MSG: return V2gMessageId::PaymentDetailsReq;
    case V2G_AUTHORIZATION_MSG: return V2gMessageId::AuthorizationReq;
    case V2G_CHARGE_PARAMETER_DISCOVERY_MSG: return V2gMessageId::ChargeParameterDiscoveryReq;
    case V2G_METERING_RECEIPT_MSG: return V2gMessageId::MeteringReceiptReq;
    case V2G_CERTIFICATE_UPDATE_MSG: return V2gMessageId::CertificateUpdateReq;
    case V2G_CERTIFICATE_INSTALLATION_MSG: return V2gMessageId::CertificateInstallationReq;
    case V2G_CHARGING_STATUS_MSG: return V2gMessageId::ChargingStatusReq;
    case V2G_CABLE_CHECK_MSG: return V2gMessageId::CableCheckReq;
    case V2G_PRE_CHARGE_MSG: return V2gMessageId::PreChargeReq;
    case V2G_POWER_DELIVERY_MSG: return V2gMessageId::PowerDeliveryReq;
    case V2G_CURRENT_DEMAND_MSG: return V2gMessageId::CurrentDemandReq;
    case V2G_WELDING_DETECTION_MSG: return V2gMessageId::WeldingDetectionReq;
    case V2G_SESSION_STOP_MSG: return V2gMessageId::SessionStopReq;
    case V2G_UNKNOWN_MSG:
    default:
        return V2gMessageId::UnknownMessage;
    }
}

/**
 * @brief Convert an ISO 15118-2 V2G response message into its matching V2gMessageId.
 * @param message_type The V2G message type to convert.
 * @return The equivalent V2gMessageId for the message type.
 */
static types::iso15118::V2gMessageId get_iso2_response_id(const V2gMsgTypeId message_type) {
    using types::iso15118::V2gMessageId;

    switch (message_type) {
    case V2G_SUPPORTED_APP_PROTOCOL_MSG: return V2gMessageId::SupportedAppProtocolRes;
    case V2G_SESSION_SETUP_MSG: return V2gMessageId::SessionSetupRes;
    case V2G_SERVICE_DISCOVERY_MSG: return V2gMessageId::ServiceDiscoveryRes;
    case V2G_SERVICE_DETAIL_MSG: return V2gMessageId::ServiceDetailRes;
    case V2G_PAYMENT_SERVICE_SELECTION_MSG: return V2gMessageId::PaymentServiceSelectionRes;
    case V2G_PAYMENT_DETAILS_MSG: return V2gMessageId::PaymentDetailsRes;
    case V2G_AUTHORIZATION_MSG: return V2gMessageId::AuthorizationRes;
    case V2G_CHARGE_PARAMETER_DISCOVERY_MSG: return V2gMessageId::ChargeParameterDiscoveryRes;
    case V2G_METERING_RECEIPT_MSG: return V2gMessageId::MeteringReceiptRes;
    case V2G_CERTIFICATE_UPDATE_MSG: return V2gMessageId::CertificateUpdateRes;
    case V2G_CERTIFICATE_INSTALLATION_MSG: return V2gMessageId::CertificateInstallationRes;
    case V2G_CHARGING_STATUS_MSG: return V2gMessageId::ChargingStatusRes;
    case V2G_CABLE_CHECK_MSG: return V2gMessageId::CableCheckRes;
    case V2G_PRE_CHARGE_MSG: return V2gMessageId::PreChargeRes;
    case V2G_POWER_DELIVERY_MSG: return V2gMessageId::PowerDeliveryRes;
    case V2G_CURRENT_DEMAND_MSG: return V2gMessageId::CurrentDemandRes;
    case V2G_WELDING_DETECTION_MSG: return V2gMessageId::WeldingDetectionRes;
    case V2G_SESSION_STOP_MSG: return V2gMessageId::SessionStopRes;
    case V2G_UNKNOWN_MSG:
    default:
        return V2gMessageId::UnknownMessage;
    }
}

/// \brief Converts the given iso2_responseCodeType \p response_code to a human-readable string
/// \return a string representation of the iso2_responseCodeType
static std::string iso2_response_code_to_string(const iso2_responseCodeType response_code) {
    return iso2_response_code_names[response_code];
}

//=============================================
//             Report Handlers
//=============================================

void report_connection_closed(const v2g_connection* conn, types::test_report::ReportMetadata metadata) {
    prepare_metadata(metadata);
    const auto report = build_connection_closed_report(metadata);
    conn->ctx->test_data.reports.push_back(report);
}

void report_connection_closed(const v2g_connection* conn) {
    report_connection_closed(conn, types::test_report::ReportMetadata());
}

void report_bsp_event(const v2g_connection* conn, const types::board_support_common::BspEvent bsp_event,
                      types::test_report::ReportMetadata metadata) {
    prepare_metadata(metadata);
    const auto report = build_bsp_event_report(bsp_event, metadata);
    conn->ctx->test_data.reports.push_back(report);
}

void report_bsp_event(const v2g_connection* conn, const types::board_support_common::BspEvent bsp_event) {
    report_bsp_event(conn, bsp_event, types::test_report::ReportMetadata());
}

void report_bsp_measurement(const v2g_connection* conn,
                            const types::board_support_common::BspMeasurement bsp_measurement,
                            types::test_report::ReportMetadata metadata) {
    prepare_metadata(metadata);
    const auto report = build_bsp_measurement_report(bsp_measurement, metadata);
    conn->ctx->test_data.reports.push_back(report);
}

void report_bsp_measurement(const v2g_connection* conn,
                            const types::board_support_common::BspMeasurement bsp_measurement) {
    report_bsp_measurement(conn, bsp_measurement, types::test_report::ReportMetadata());
}

void report_v2g_message(const v2g_connection* conn, const types::iso15118::V2gMessageId message_id,
                        V2gMsgReportContext context) {
    prepare_metadata(context.metadata);
    const auto report = build_v2g_message_report(message_id, context.message_fields, context.metadata);
    conn->ctx->test_data.reports.push_back(report);
}

void report_v2g_message(const v2g_connection* conn, const types::iso15118::V2gMessageId message_id) {
    report_v2g_message(conn, message_id, V2gMsgReportContext());
}

void report_din_request(const v2g_connection* conn, const V2gMsgTypeId message_type,
                        const V2gMsgReportContext& context) {
    const auto message_id = get_din_request_id(message_type);
    report_v2g_message(conn, message_id, context);
}

void report_din_request(const v2g_connection* conn, const V2gMsgTypeId message_type) {
    report_din_request(conn, message_type, V2gMsgReportContext());
}

void report_din_response(const v2g_connection* conn, const V2gMsgTypeId message_type,
                         V2gResReportContext<din_responseCodeType> context) {
    const auto message_id = get_din_response_id(message_type);

    if (context.response_code) {
        const auto response_code = din_response_code_to_string(*context.response_code);
        context.message_fields.insert(context.message_fields.begin(), {
            .name = "ResponseCode", .value = response_code
        });
    }

    report_v2g_message(conn, message_id, {
        .message_fields = context.message_fields,
        .metadata = context.metadata
    });
}

void report_din_response(const v2g_connection* conn, const V2gMsgTypeId message_type) {
    report_din_response(conn, message_type, V2gResReportContext<din_responseCodeType>());
}

void report_iso2_request(const v2g_connection* conn, const V2gMsgTypeId message_type,
                         const V2gMsgReportContext& context) {
    const auto message_id = get_iso2_request_id(message_type);
    report_v2g_message(conn, message_id, context);
}

void report_iso2_request(const v2g_connection* conn, const V2gMsgTypeId message_type) {
    report_iso2_request(conn, message_type, V2gMsgReportContext());
}

void report_iso2_response(const v2g_connection* conn, const V2gMsgTypeId message_type,
                          V2gResReportContext<iso2_responseCodeType> context) {
    const auto message_id = get_iso2_response_id(message_type);

    if (context.response_code) {
        const auto response_code = iso2_response_code_to_string(*context.response_code);
        context.message_fields.insert(context.message_fields.begin(), {
            .name = "ResponseCode", .value = response_code
        });
    }

    report_v2g_message(conn, message_id, V2gMsgReportContext{
        .message_fields = context.message_fields,
        .metadata = context.metadata
    });
}

void report_iso2_response(const v2g_connection* conn, const V2gMsgTypeId message_type) {
    report_iso2_response(conn, message_type, V2gResReportContext<iso2_responseCodeType>());
}

} // namespace testing
