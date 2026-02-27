// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 chargebyte GmbH
// Copyright (C) 2023 Contributors to EVerest

#include "v2g_server.hpp"

#include "chn_authorization_004.hpp"
#include "chn_authorization_009.hpp"
#include "chn_cable_check_006.hpp"
#include "chn_cablecheck_007.hpp"
#include "chn_charge_parameter_discovery_002.hpp"
#include "chn_current_demand_002.hpp"
#include "chn_current_demand_005.hpp"
#include "chn_current_demand_007.hpp"
#include "chn_precharge_006.hpp"
#include "chn_service_detail_011.hpp"
#include "chn_service_detail_013.hpp"
#include "chn_service_detail_payment_selection_003.hpp"
#include "chn_service_detail_payment_selection_012.hpp"
#include "chn_service_discovery_008.hpp"
#include "chn_session_setup_004.hpp"
#include "chn_session_setup_007.hpp"
#include "chn_welding_session_stop_001.hpp"
#include "chx_smart_charging_scheduling_001.hpp"
#include "chx_smart_charging_scheduling_002.hpp"
#include "chx_smart_charging_scheduling_003.hpp"

#include <cstdint>
#include <cstdlib>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <cbv2g/app_handshake/appHand_Decoder.h>
#include <cbv2g/app_handshake/appHand_Encoder.h>
#include <cbv2g/common/exi_basetypes.h>
#include <cbv2g/din/din_msgDefDecoder.h>
#include <cbv2g/din/din_msgDefEncoder.h>
#include <cbv2g/exi_v2gtp.h>
#include <cbv2g/iso_2/iso2_msgDefDecoder.h>
#include <cbv2g/iso_2/iso2_msgDefEncoder.h>

#include "TestWrapperImpl.hpp"
#include "connection.hpp"
#include "abc_server.hpp"
#include "log.hpp"
#include "testcase.hpp"
#include "tools.hpp"

#define MAX_RES_TIME 98

static types::iso15118::V2gMessageId get_v2g_message_id(const V2gMsgTypeId v2g_msg,
                                                        const v2g_protocol selected_protocol,
                                                        const bool is_req) {
    switch (v2g_msg) {
    case V2G_SUPPORTED_APP_PROTOCOL_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::SupportedAppProtocolReq
                              : types::iso15118::V2gMessageId::SupportedAppProtocolRes;
    case V2G_SESSION_SETUP_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::SessionSetupReq
                              : types::iso15118::V2gMessageId::SessionSetupRes;
    case V2G_SERVICE_DISCOVERY_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::ServiceDiscoveryReq
                              : types::iso15118::V2gMessageId::ServiceDiscoveryRes;
    case V2G_SERVICE_DETAIL_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::ServiceDetailReq
                              : types::iso15118::V2gMessageId::ServiceDetailRes;
    case V2G_PAYMENT_SERVICE_SELECTION_MSG:
        return is_req == true ? selected_protocol == V2G_PROTO_DIN70121
                                  ? types::iso15118::V2gMessageId::ServicePaymentSelectionReq
                                  : types::iso15118::V2gMessageId::PaymentServiceSelectionReq
                              : selected_protocol == V2G_PROTO_DIN70121
                                  ? types::iso15118::V2gMessageId::ServicePaymentSelectionRes
                                  : types::iso15118::V2gMessageId::PaymentServiceSelectionRes;
    case V2G_PAYMENT_DETAILS_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::PaymentDetailsReq
                              : types::iso15118::V2gMessageId::PaymentDetailsRes;
    case V2G_AUTHORIZATION_MSG:
        return is_req == true ? selected_protocol == V2G_PROTO_DIN70121
                                  ? types::iso15118::V2gMessageId::ContractAuthenticationReq
                                  : types::iso15118::V2gMessageId::AuthorizationReq
                              : selected_protocol == V2G_PROTO_DIN70121
                                  ? types::iso15118::V2gMessageId::ContractAuthenticationRes
                                  : types::iso15118::V2gMessageId::AuthorizationRes;
    case V2G_CHARGE_PARAMETER_DISCOVERY_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::ChargeParameterDiscoveryReq
                              : types::iso15118::V2gMessageId::ChargeParameterDiscoveryRes;
    case V2G_METERING_RECEIPT_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::MeteringReceiptReq
                              : types::iso15118::V2gMessageId::MeteringReceiptRes;
    case V2G_CERTIFICATE_UPDATE_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::CertificateUpdateReq
                              : types::iso15118::V2gMessageId::CertificateUpdateRes;
    case V2G_CERTIFICATE_INSTALLATION_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::CertificateInstallationReq
                              : types::iso15118::V2gMessageId::CertificateInstallationRes;
    case V2G_CHARGING_STATUS_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::ChargingStatusReq
                              : types::iso15118::V2gMessageId::ChargingStatusRes;
    case V2G_CABLE_CHECK_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::CableCheckReq
                              : types::iso15118::V2gMessageId::CableCheckRes;
    case V2G_PRE_CHARGE_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::PreChargeReq
                              : types::iso15118::V2gMessageId::PreChargeRes;
    case V2G_POWER_DELIVERY_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::PowerDeliveryReq
                              : types::iso15118::V2gMessageId::PowerDeliveryRes;
    case V2G_CURRENT_DEMAND_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::CurrentDemandReq
                              : types::iso15118::V2gMessageId::CurrentDemandRes;
    case V2G_WELDING_DETECTION_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::WeldingDetectionReq
                              : types::iso15118::V2gMessageId::WeldingDetectionRes;
    case V2G_SESSION_STOP_MSG:
        return is_req == true ? types::iso15118::V2gMessageId::SessionStopReq
                              : types::iso15118::V2gMessageId::SessionStopRes;
    case V2G_UNKNOWN_MSG:
    default:
        return types::iso15118::V2gMessageId::UnknownMessage;
    }
}

/**
 * @brief This function fills a V2gMessages type with the V2G EXI message as HEX and Base64.
 * @param conn Holds the context of the V2G-connection.
 * @param is_req Whether this is a V2G request or response: <c>true</c> for a request, and <c>false</c> for a response.
 */
static void publish_var_V2G_Message(const v2g_connection* conn, const bool is_req) {
    types::iso15118::V2gMessages v2g_message;

    u_int8_t* tempbuff = conn->buffer;
    std::string msg_as_hex_string;
    for (int i = 0; ((tempbuff != nullptr) && (i < conn->payload_len + V2GTP_HEADER_LENGTH)); i++) {
        char hex[4];
        snprintf(hex, 4, "%x", *tempbuff); // to hex
        if (std::string(hex).size() == 1)
            msg_as_hex_string += '0';
        msg_as_hex_string += hex;
        tempbuff++;
    }

    std::string EXI_Base64;

    EXI_Base64 = openssl::base64_encode(conn->buffer, conn->payload_len + V2GTP_HEADER_LENGTH);
    if (EXI_Base64.size() == 0) {
        dlog(DLOG_LEVEL_WARNING, "Unable to base64 encode EXI buffer");
    }

    v2g_message.exi_base64 = EXI_Base64;
    v2g_message.id = get_v2g_message_id(conn->ctx->current_v2g_msg, conn->ctx->selected_protocol, is_req);
    v2g_message.exi = msg_as_hex_string;
    conn->ctx->p_charger->publish_v2g_messages(v2g_message);
}

/**
 * @brief This function publishes the state of a running test case.
 * @param conn Holds the context of the V2G-connection.
 * @param state V2G test state to publish.
 */
static void publish_v2g_test_status(const v2g_connection* conn,
                                    const types::evse_test_common::TestState state) {
    conn->ctx->p_tester->publish_test_status({
        .status = state,
    });
}

static void publish_v2g_test_result(const v2g_connection* conn) {

    std::optional<types::evse_test_common::V2GProtocol> protocol;
    std::optional<types::evse_test_common::ChargeMode> charge_mode;
    std::optional<types::evse_test_common::IdentificationMode> ident_mode;

    switch (conn->ctx->selected_protocol) {
        case V2G_PROTO_DIN70121:
        case V2G_PROTO_ISO15118_2010:
            protocol = types::evse_test_common::V2GProtocol::DIN_70121;
            charge_mode = types::evse_test_common::ChargeMode::DC;
            ident_mode = types::evse_test_common::IdentificationMode::EIM;
            break;
        case V2G_PROTO_ISO15118_2013:
            protocol = types::evse_test_common::V2GProtocol::ISO_15118_2;
            if (conn->ctx->is_dc_charger) {
                charge_mode = types::evse_test_common::ChargeMode::DC;
            } else {
                charge_mode = types::evse_test_common::ChargeMode::AC;
            }
            switch (conn->ctx->session.iso_selected_payment_option) {
                case iso2_paymentOptionType_Contract:
                    ident_mode = types::evse_test_common::IdentificationMode::PnC;
                    break;
                case iso2_paymentOptionType_ExternalPayment:
                    ident_mode = types::evse_test_common::IdentificationMode::EIM;
                    break;
            }
            break;
        default:
            break;
    }

    conn->ctx->p_tester->publish_test_result({
        .outcome = conn->ctx->test_data.outcome,
        .context = {
            .protocol = protocol,
            .charge_mode = charge_mode,
            .ident_mode = ident_mode,
        },
        .evidence = conn->ctx->test_data.evidence,
        .reports = conn->ctx->test_data.reports,
        .errors = conn->ctx->test_data.errors,
    });
}

static void update_heartbeat(const v2g_connection* conn) {
    const auto now = getmonotonictime();
    const auto time_since_heartbeat = now - conn->ctx->last_heartbeat_time;

    if (time_since_heartbeat >= HEARTBEAT_UPDATE_RATE) {
        conn->ctx->p_tester->publish_heartbeat(nullptr);
        conn->ctx->last_heartbeat_time = now;
    }
}

/**
 * @brief This function reads the V2G transport header.
 * @param conn Holds the context of the V2G-connection.
 * @return Returns <c>0</c> if the V2G-session was successfully stopped, otherwise <c>-1</c>.
 */
static int v2g_incoming_v2gtp(v2g_connection* conn) {
    assert(conn != nullptr);
    assert(conn->read != nullptr);

    int rv;

    /* read and process header */
    rv = conn->read(conn, conn->buffer, V2GTP_HEADER_LENGTH);
    if (rv < 0) {
        dlog(DLOG_LEVEL_ERROR, "connection_read(header) failed: %s",
             (rv == -1) ? strerror(errno) : "connection terminated");
        return -1;
    }
    /* peer closed connection */
    if (rv == 0)
        return 1;
    if (rv != V2GTP_HEADER_LENGTH) {
        dlog(DLOG_LEVEL_ERROR, "connection_read(header) too short: expected %d, got %d", V2GTP_HEADER_LENGTH, rv);
        return -1;
    }

    rv = V2GTP_ReadHeader(conn->buffer, &conn->payload_len);
    if (rv == -1) {
        dlog(DLOG_LEVEL_ERROR, "Invalid v2gtp header");
        return -1;
    }

    if (conn->payload_len >= UINT32_MAX - V2GTP_HEADER_LENGTH) {
        dlog(DLOG_LEVEL_ERROR, "Prevent integer overflow - payload too long: have %d, would need %u",
             DEFAULT_BUFFER_SIZE, conn->payload_len);
        return -1;
    }

    if (conn->payload_len + V2GTP_HEADER_LENGTH > DEFAULT_BUFFER_SIZE) {
        dlog(DLOG_LEVEL_ERROR, "payload too long: have %d, would need %u", DEFAULT_BUFFER_SIZE,
             conn->payload_len + V2GTP_HEADER_LENGTH);

        /* we have no way to flush/discard remaining unread data from the socket without reading it in chunks,
         * but this opens the chance to bind us in an "endless" read loop; so to protect us, simply close the connection
         */

        return -1;
    }

    /* read request */
    rv = conn->read(conn, &conn->buffer[V2GTP_HEADER_LENGTH], conn->payload_len);
    if (rv < 0) {
        dlog(DLOG_LEVEL_ERROR, "connection_read(payload) failed: %s",
             (rv == -1) ? strerror(errno) : "connection terminated");
        return -1;
    }
    if (rv != conn->payload_len) {
        dlog(DLOG_LEVEL_ERROR, "connection_read(payload) too short: expected %d, got %d", conn->payload_len, rv);
        return -1;
    }

    /* adjust buffer pos to decode request */
    conn->stream.byte_pos = V2GTP_HEADER_LENGTH;
    conn->stream.data_size = conn->payload_len + V2GTP_HEADER_LENGTH;

    return 0;
}

/**
 * @brief This function creates the v2g transport header.
 * @param conn Holds the context of the V2G-connection.
 * @return Returns <c>0</c> if the v2g-session was successfully stopped, otherwise <c>-1</c>.
 */
int v2g_outgoing_v2gtp(v2g_connection* conn) {
    assert(conn != nullptr);
    assert(conn->write != nullptr);

    /* fixup/create header */
    const auto len = exi_bitstream_get_length(&conn->stream);

    V2GTP_WriteHeader(conn->buffer, len - V2GTP_HEADER_LENGTH);

    if (conn->write(conn, conn->buffer, len) == -1) {
        dlog(DLOG_LEVEL_ERROR, "connection_write(header) failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief After receiving a supportedAppProtocolReq message,
 * the SECC shall process the received information. DIN [V2G-DC-436] ISO [V2G2-540]
 * @param conn Holds the context of the V2G-connection.
 * @return Returns a v2g-event of type enum v2g_event.
 */
static v2g_event v2g_handle_apphandshake(v2g_connection* conn) {
    v2g_event next_event = V2G_EVENT_NO_EVENT;
    int i;
    uint8_t ev_app_priority = 20; // lowest priority

    /* validate handshake request and create response */
    init_appHand_exiDocument(&conn->handshake_resp);
    conn->handshake_resp.supportedAppProtocolRes_isUsed = 1;
    conn->handshake_resp.supportedAppProtocolRes.ResponseCode =
        appHand_responseCodeType_Failed_NoNegotiation; // [V2G2-172]

    dlog(DLOG_LEVEL_INFO, "Handling SupportedAppProtocolReq");
    conn->ctx->current_v2g_msg = V2G_SUPPORTED_APP_PROTOCOL_MSG;

    if (decode_appHand_exiDocument(&conn->stream, &conn->handshake_req) != 0) {
        dlog(DLOG_LEVEL_ERROR, "decode_appHandExiDocument() failed");
        // If the message can't be decoded we have to terminate the tcp-connection (e.g. after an unexpected message)
        return V2G_EVENT_TERMINATE_CONNECTION;
    }

    types::iso15118::AppProtocols app_protocols; // to publish supported app protocol array

    // A bit-mask that holds the app protocols mutually supported by the EVSE config and current test case
    const auto supported_protocols = conn->ctx->supported_protocols & conn->ctx->test_data.supported_protocols;

    const bool auth_eim_listed =
                conn->ctx->evse_v2g_data.payment_option_list[0] == iso2_paymentOptionType_ExternalPayment or
                conn->ctx->evse_v2g_data.payment_option_list[1] == iso2_paymentOptionType_ExternalPayment;
    const bool auth_pnc_listed =
        conn->ctx->evse_v2g_data.payment_option_list[0] == iso2_paymentOptionType_Contract or
        conn->ctx->evse_v2g_data.payment_option_list[1] == iso2_paymentOptionType_Contract;

    for (i = 0; i < conn->handshake_req.supportedAppProtocolReq.AppProtocol.arrayLen; i++) {
        const appHand_AppProtocolType* app_proto = &conn->handshake_req.supportedAppProtocolReq.AppProtocol.array[i];
        char* proto_ns = strndup(app_proto->ProtocolNamespace.characters,
                                 app_proto->ProtocolNamespace.charactersLen);

        if (!proto_ns) {
            dlog(DLOG_LEVEL_ERROR, "out-of-memory condition");
            return V2G_EVENT_TERMINATE_CONNECTION;
        }

        dlog(DLOG_LEVEL_TRACE,
             "handshake_req: Namespace: %s, Version: %" PRIu32 ".%" PRIu32 ", SchemaID: %" PRIu8 ", Priority: %" PRIu8,
             proto_ns, app_proto->VersionNumberMajor, app_proto->VersionNumberMinor, app_proto->SchemaID,
             app_proto->Priority);

        if ((supported_protocols & (1 << V2G_PROTO_DIN70121)) and
            (strcmp(proto_ns, DIN_70121_MSG_DEF) == 0) and
            (app_proto->VersionNumberMajor == DIN_70121_MAJOR) and
            (ev_app_priority >= app_proto->Priority)) {

            // Ignore if required payment options are disjoint from test constraints
            if (!(conn->ctx->test_data.auth_eim_supported and auth_eim_listed)) {
                if (!conn->ctx->test_data.auth_eim_supported) {
                    // The EIM identification mode is disabled by the active test config
                    dlog(DLOG_LEVEL_ERROR, "Ignoring DIN 70121 AppProtocol due to EIM payment unsupported by test config.");
                    conn->ctx->test_data.errors.emplace_back("DIN 70121 AppProtocol is unavailable due to EIM payment unsupported by test config");
                } else {
                    // The EIM identification mode was not listed within the available payment options set by the EVSE
                    dlog(DLOG_LEVEL_WARNING, "Ignoring DIN 70121 AppProtocol due to EIM payment unsupported by EVSE.");
                    conn->ctx->test_data.errors.emplace_back("DIN 70121 AppProtocol is unavailable due to EIM payment unsupported by EVSE");
                }
            }
            // TODO(cb): Exclude if required charge mode is disjoint from test constraints
            // Select the V2G protocol
            else {
                ev_app_priority = app_proto->Priority;
                conn->handshake_resp.supportedAppProtocolRes.ResponseCode =
                    appHand_responseCodeType_OK_SuccessfulNegotiation;
                conn->handshake_resp.supportedAppProtocolRes.SchemaID = app_proto->SchemaID;
                conn->ctx->selected_protocol = V2G_PROTO_DIN70121;
            }

        } else if ((supported_protocols & (1 << V2G_PROTO_ISO15118_2013)) and
                   (strcmp(proto_ns, ISO_15118_2013_MSG_DEF) == 0) and
                   (app_proto->VersionNumberMajor == ISO_15118_2013_MAJOR) and
                   (ev_app_priority >= app_proto->Priority)) {

            // Ignore if required payment options are disjoint from test constraints
            if (!(conn->ctx->test_data.auth_pnc_supported and auth_pnc_listed and conn->is_tls_connection) and
                !(conn->ctx->test_data.auth_eim_supported and auth_eim_listed)) {

                if (!conn->ctx->test_data.auth_pnc_supported and !conn->ctx->test_data.auth_eim_supported) {
                    // All identification modes are disabled by the active test config
                    dlog(DLOG_LEVEL_ERROR, "Ignoring ISO 15118-2 AppProtocol due to all identification modes disabled by test config.");
                    conn->ctx->test_data.errors.emplace_back(
                        "ISO 15118-2 AppProtocol is unavailable due to all identification modes disabled by test config.");
                }
                else if (conn->ctx->test_data.auth_pnc_supported) {
                    if (!auth_pnc_listed) {
                        // The PnC identification mode was not listed within the available payment options set by the EVSE
                        dlog(DLOG_LEVEL_WARNING, "Ignoring ISO 15118-2 AppProtocol due to PnC payment unsupported by EVSE.");
                        conn->ctx->test_data.errors.emplace_back("ISO 15118-2 AppProtocol is unavailable due to PnC payment unsupported by EVSE");
                    } else {
                        // The PnC identification mode requires a TLS connection, but TLS was not used
                        dlog(DLOG_LEVEL_WARNING, "Ignoring ISO 15118-2 AppProtocol due to PnC requiring a TLS connection.");
                        conn->ctx->test_data.errors.emplace_back("ISO 15118-2 AppProtocol is unavailable due to PnC payment requiring TLS");
                    }
                }
                else {
                    // The EIM identification mode was not listed within the available payment options set by the EVSE
                    dlog(DLOG_LEVEL_WARNING, "Ignoring ISO 15118-2 AppProtocol due to EIM payment unsupported by EVSE.");
                    conn->ctx->test_data.errors.emplace_back("ISO 15118-2 AppProtocol is unavailable due to EIM payment unsupported by EVSE");
                }
            }
            // TODO(cb): Exclude if required charge mode is disjoint from test constraints
            // Select the V2G protocol
            else {
                ev_app_priority = app_proto->Priority;
                conn->handshake_resp.supportedAppProtocolRes.ResponseCode =
                    appHand_responseCodeType_OK_SuccessfulNegotiation;
                conn->handshake_resp.supportedAppProtocolRes.SchemaID = app_proto->SchemaID;
                conn->ctx->selected_protocol = V2G_PROTO_ISO15118_2013;
            }

        }

        if (conn->ctx->debugMode == true) {
            const types::iso15118::AppProtocol protocol = {
                std::string(proto_ns), static_cast<int32_t>(app_proto->VersionNumberMajor),
                static_cast<int32_t>(app_proto->VersionNumberMinor), static_cast<int32_t>(app_proto->SchemaID),
                static_cast<int32_t>(app_proto->Priority)};

            app_protocols.Protocols.push_back(protocol);
        }

        // TODO: ISO15118v2
        free(proto_ns);
    }

    if (conn->ctx->debugMode == true) {
        conn->ctx->p_charger->publish_ev_app_protocol(app_protocols);
        /* form the content of V2G_Message type and publish the request*/
        publish_var_V2G_Message(conn, true);
    }

    std::string selected_protocol_str;
    if (conn->handshake_resp.supportedAppProtocolRes.ResponseCode ==
        appHand_responseCodeType_OK_SuccessfulNegotiation) {
        conn->handshake_resp.supportedAppProtocolRes.SchemaID_isUsed = static_cast<unsigned int>(1);
        if (V2G_PROTO_DIN70121 == conn->ctx->selected_protocol) {
            dlog(DLOG_LEVEL_INFO, "Protocol negotiation was successful. Selected protocol is DIN70121");
            selected_protocol_str = "DIN70121";
        } else if (V2G_PROTO_ISO15118_2013 == conn->ctx->selected_protocol) {
            dlog(DLOG_LEVEL_INFO, "Protocol negotiation was successful. Selected protocol is ISO15118");
            selected_protocol_str = "ISO15118-2-2013";
        } else if (V2G_PROTO_ISO15118_2010 == conn->ctx->selected_protocol) {
            dlog(DLOG_LEVEL_INFO, "Protocol negotiation was successful. Selected protocol is ISO15118-2010");
            selected_protocol_str = "ISO15118-2-2010";
        }
    } else {
        dlog(DLOG_LEVEL_ERROR, "No compatible protocol found");
        conn->ctx->test_data.errors.emplace_back("No compatible AppProtocol found");
        selected_protocol_str = "None";
        next_event = V2G_EVENT_SEND_AND_TERMINATE; // Send response and terminate tcp-connection
    }

    if (conn->ctx->debugMode == true) {
        conn->ctx->p_charger->publish_selected_protocol(selected_protocol_str);
    }

    if (conn->ctx->is_connection_terminated == true) {
        dlog(DLOG_LEVEL_ERROR, "Connection is terminated. Abort charging");
        return V2G_EVENT_TERMINATE_CONNECTION; // Abort charging without sending a response
    }

    /* Validate response code */
    if ((conn->ctx->intl_emergency_shutdown == true) or
        (conn->ctx->stop_hlc == true) or
        (V2G_EVENT_SEND_AND_TERMINATE == next_event)) {

        conn->handshake_resp.supportedAppProtocolRes.ResponseCode = appHand_responseCodeType_Failed_NoNegotiation;
        dlog(DLOG_LEVEL_ERROR, "Abort charging session");

        if (conn->ctx->terminate_connection_on_failed_response == true) {
            next_event = V2G_EVENT_SEND_AND_TERMINATE; // send response and terminate the TCP-connection
        }
    }

    /* encode response at the right buffer location */
    conn->stream.byte_pos = V2GTP_HEADER_LENGTH;
    conn->stream.bit_count = 0;

    if (encode_appHand_exiDocument(&conn->stream, &conn->handshake_resp) != 0) {
        dlog(DLOG_LEVEL_ERROR, "Encoding of the protocol handshake message failed");
        next_event = V2G_EVENT_SEND_AND_TERMINATE;
    }

    return next_event;
}

/**
 * @brief Returns a DIN SPEC 70121 server for a test case.
 * @param test_id The static test ID to create a DIN server for.
 * @return The newly created DIN server or <c>nullptr</c> if missing.
 */
std::unique_ptr<testing::Test> v2g_create_din_server(const types::evse_test_common::TestId test_id) {
    using namespace types::evse_test_common;
    switch (test_id) {

    case TestId::V2GT_TC_CHN_SessionSetup_004:
        return std::make_unique<testing::chn_session_setup_004::DinTestServer>();
    case TestId::V2GT_TC_CHN_SessionSetup_007:
        return std::make_unique<testing::chn_session_setup_007::DinTestServer>();
    case TestId::V2GT_TC_CHN_ServiceDiscovery_008:
        return std::make_unique<testing::chn_service_discovery_008::DinTestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetailAndPaymentSelection_003:
        return std::make_unique<testing::chn_service_detail_payment_selection_003::DinTestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetailAndPaymentSelection_012:
        return std::make_unique<testing::chn_service_detail_payment_selection_012::DinTestServer>();
    case TestId::V2GT_TC_CHN_Authorization_004:
        return std::make_unique<testing::chn_authorization_004::DinTestServer>();
    case TestId::V2GT_TC_CHN_Authorization_009:
        return std::make_unique<testing::chn_authorization_009::DinTestServer>();
    case TestId::V2GT_TC_CHN_ChargeParameterDiscovery_002:
        return std::make_unique<testing::chn_charge_parameter_discovery_002::DinTestServer>();
    case TestId::V2GT_TC_CHN_CableCheck_006:
        return std::make_unique<testing::chn_cable_check_006::DinTestServer>();
    case TestId::V2GT_TC_CHN_CableCheck_007:
        return std::make_unique<testing::chn_cablecheck_007::DinTestServer>();
    case TestId::V2GT_TC_CHN_PreCharge_006:
        return std::make_unique<testing::chn_precharge_006::DinTestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_002:
        return std::make_unique<testing::chn_current_demand_002::DinTestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_005:
        return std::make_unique<testing::chn_current_demand_005::DinTestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_007:
        return std::make_unique<testing::chn_current_demand_007::DinTestServer>();
    case TestId::V2GT_TC_CHN_WeldingDetectionOrSessionStop_001:
        return std::make_unique<testing::chn_welding_session_stop_001::DinTestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_001:
        return std::make_unique<testing::chx_smart_charging_scheduling_001::DinTestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_002:
        return std::make_unique<testing::chx_smart_charging_scheduling_002::DinTestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_003:
        return std::make_unique<testing::chx_smart_charging_scheduling_003::DinTestServer>();

    default:
        return nullptr;
    }
}

/**
 * @brief Returns an ISO 15118-2 server for a test case.
 * @param test_id The static test ID to create a ISO server for.
 * @return The newly created ISO server or <c>nullptr</c> if missing.
 */
std::unique_ptr<testing::Test> v2g_create_iso_server(const types::evse_test_common::TestId test_id) {
    using namespace types::evse_test_common;
    switch (test_id) {

    case TestId::V2GT_TC_CHN_SessionSetup_004:
        return std::make_unique<testing::chn_session_setup_004::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_SessionSetup_007:
        return std::make_unique<testing::chn_session_setup_007::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ServiceDiscovery_008:
        return std::make_unique<testing::chn_service_discovery_008::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetail_011:
        return std::make_unique<testing::chn_service_detail_011::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetail_013:
        return std::make_unique<testing::chn_service_detail_013::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetailAndPaymentSelection_003:
        return std::make_unique<testing::chn_service_detail_payment_selection_003::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ServiceDetailAndPaymentSelection_012:
        return std::make_unique<testing::chn_service_detail_payment_selection_012::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_Authorization_004:
        return std::make_unique<testing::chn_authorization_004::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_ChargeParameterDiscovery_002:
        return std::make_unique<testing::chn_charge_parameter_discovery_002::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_CableCheck_006:
        return std::make_unique<testing::chn_cable_check_006::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_CableCheck_007:
        return std::make_unique<testing::chn_cablecheck_007::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_PreCharge_006:
        return std::make_unique<testing::chn_precharge_006::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_002:
        return std::make_unique<testing::chn_current_demand_002::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_005:
        return std::make_unique<testing::chn_current_demand_005::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_CurrentDemand_007:
        return std::make_unique<testing::chn_current_demand_007::Iso2TestServer>();
    case TestId::V2GT_TC_CHN_WeldingDetectionOrSessionStop_001:
        return std::make_unique<testing::chn_welding_session_stop_001::Iso2TestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_001:
        return std::make_unique<testing::chx_smart_charging_scheduling_001::Iso2TestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_002:
        return std::make_unique<testing::chx_smart_charging_scheduling_002::Iso2TestServer>();
    case TestId::V2GT_TC_CHX_SmartChargingScheduling_003:
        return std::make_unique<testing::chx_smart_charging_scheduling_003::Iso2TestServer>();

    default:
        return nullptr;
    }
}

int v2g_handle_connection(v2g_connection* conn) {

#pragma region Setup
    v2g_protocol selected_protocol = V2G_UNKNOWN_PROTOCOL;
    v2g_event rvAppHandshake = V2G_EVENT_NO_EVENT;
    bool stop_receiving_loop = false;
    int64_t start_time = 0; // in milliseconds
    int rv = -1;

    std::unique_ptr<testing::Test> server;
    std::unique_ptr<testing::TestWrapperImpl> test_wrapper;

    // TODO: additional considerations needed
    publish_v2g_test_status(conn, types::evse_test_common::TestState::Running);
    conn->ctx->test_data.outcome = types::evse_test_common::TestOutcome::PreconditionsNotMet;

    update_heartbeat(conn);

    v2g_ctx_init_charging_state(conn->ctx, false);
    conn->buffer = static_cast<uint8_t*>(malloc(DEFAULT_BUFFER_SIZE));
    if (!conn->buffer) {
        // TODO: additional considerations needed
        publish_v2g_test_status(conn, types::evse_test_common::TestState::Finished);
        return -1;
    }

    /* static setup */
    conn->stream.data = conn->buffer;
#pragma endregion

    /* Here is a good point to wait until the customer is ready for a resumed session,
     * because we are waiting for the incoming message of the ev */
    if (conn->dlink_action == MQTT_DLINK_ACTION_PAUSE) {
        // TODO: D_LINK pause
    }

#pragma region Handle Protocol Handshake
    do {
        /* setup for receive */
        conn->stream.data[0] = 0;
        conn->payload_len = 0;
        exi_bitstream_init(&conn->stream, conn->buffer, 0, 0, nullptr);

        update_heartbeat(conn);

        /* next call return -1 on error, 1 when peer closed connection, 0 on success */
        rv = v2g_incoming_v2gtp(conn);

        if (rv != 0) {
            dlog(DLOG_LEVEL_ERROR, "v2g_incoming_v2gtp() failed");
            goto error_out;
        }

        if (conn->ctx->is_connection_terminated == true) {
            rv = -1;
            goto error_out;
        }

        /* next call return -1 on non-recoverable errors, 1 on recoverable errors, 0 on success */
        rvAppHandshake = v2g_handle_apphandshake(conn);

        if (rvAppHandshake == V2G_EVENT_IGNORE_MSG) {
            dlog(DLOG_LEVEL_WARNING, "v2g_handle_apphandshake() failed, ignoring packet");
        }
    } while ((rv == 1) and (rvAppHandshake == V2G_EVENT_IGNORE_MSG));

    /* stream setup for sending is done within v2g_handle_apphandshake */
    /* send supportedAppRes message */
    if ((rvAppHandshake == V2G_EVENT_SEND_AND_TERMINATE) or (rvAppHandshake == V2G_EVENT_NO_EVENT)) {
        /* form the content of V2G_Message type and publish the response for debugging*/
        if (conn->ctx->debugMode == true) {
            publish_var_V2G_Message(conn, false);
        }

        rv = v2g_outgoing_v2gtp(conn);

        if (rv == -1) {
            dlog(DLOG_LEVEL_ERROR, "v2g_outgoing_v2gtp() failed");
            goto error_out;
        }
    }

    /* terminate connection, if supportedApp handshake has failed */
    if ((rvAppHandshake == V2G_EVENT_SEND_AND_TERMINATE) or (rvAppHandshake == V2G_EVENT_TERMINATE_CONNECTION)) {
        rv = -1;
        goto error_out;
    }

    /* Backup the selected protocol, because this value is shared and can be reset while unplugging. */
    selected_protocol = conn->ctx->selected_protocol;

    update_heartbeat(conn);
#pragma endregion

#pragma region Allocate In/Out Resources
    /* allocate in/out documents dynamically */
    switch (selected_protocol) {
    case V2G_PROTO_DIN70121:
    case V2G_PROTO_ISO15118_2010:
        conn->exi_in.dinEXIDocument = static_cast<din_exiDocument*>(calloc(1, sizeof(din_exiDocument)));
        if (conn->exi_in.dinEXIDocument == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "out-of-memory");
            goto error_out;
        }
        conn->exi_out.dinEXIDocument = static_cast<din_exiDocument*>(calloc(1, sizeof(din_exiDocument)));
        if (conn->exi_out.dinEXIDocument == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "out-of-memory");
            goto error_out;
        }
        server = v2g_create_din_server(conn->ctx->test_data.test_id);
        if (server == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "missing-test-server");
            conn->ctx->test_data.errors.emplace_back("Test is not implemented");
            goto error_out;
        }
        break;
    case V2G_PROTO_ISO15118_2013:
        conn->exi_in.iso2EXIDocument = static_cast<iso2_exiDocument*>(calloc(1, sizeof(iso2_exiDocument)));
        if (conn->exi_in.iso2EXIDocument == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "out-of-memory");
            goto error_out;
        }
        conn->exi_out.iso2EXIDocument = static_cast<iso2_exiDocument*>(calloc(1, sizeof(iso2_exiDocument)));
        if (conn->exi_out.iso2EXIDocument == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "out-of-memory");
            goto error_out;
        }
        server = v2g_create_iso_server(conn->ctx->test_data.test_id);
        if (server == nullptr) {
            dlog(DLOG_LEVEL_ERROR, "missing-test-server");
            conn->ctx->test_data.errors.emplace_back("Test is not implemented");
            goto error_out;
        }
        break;
    default:
        goto error_out; // protocol is unknown
    }

    test_wrapper = std::make_unique<testing::TestWrapperImpl>(conn, server.get());
    {
        std::lock_guard lock(conn->ctx->test_data.test_mutex);
        conn->ctx->test_data.test_wrapper = test_wrapper.get();
    }
    test_wrapper->dispatch_test_started();
#pragma endregion

#pragma region Handle V2G Messaging
    do {
        /* setup for receive */
        conn->stream.data[0] = 0;
        conn->stream.bit_count = 0;
        conn->stream.byte_pos = 0;
        conn->payload_len = 0;

        update_heartbeat(conn);

        /* next call return -1 on error, 1 when peer closed connection, 0 on success */
        rv = v2g_incoming_v2gtp(conn);

        if (rv == 1) {
            dlog(DLOG_LEVEL_ERROR, "Timeout waiting for next request or peer closed connection");
            break;
        } else if (rv == -1) {
            dlog(DLOG_LEVEL_ERROR, "v2g_incoming_v2gtp() (previous message \"%s\") failed",
                 v2g_msg_type[conn->ctx->last_v2g_msg]);
            break;
        }

        start_time = getmonotonictime(); // To calc the duration of req msg configuration

        /* according to agreed protocol decode the stream */
        v2g_event v2gEvent = V2G_EVENT_NO_EVENT;
        switch (selected_protocol) {
        case V2G_PROTO_DIN70121:
        case V2G_PROTO_ISO15118_2010:
            memset(conn->exi_in.dinEXIDocument, 0, sizeof(din_exiDocument));
            rv = decode_din_exiDocument(&conn->stream, conn->exi_in.dinEXIDocument);

            if (rv != 0) {
                dlog(DLOG_LEVEL_ERROR, "decode_dinExiDocument() (previous message \"%s\") failed: %d",
                     v2g_msg_type[conn->ctx->last_v2g_msg], rv);
                /* we must ignore packet which we cannot decode, so reset rv to zero to stay in loop */
                rv = 0;
                v2gEvent = V2G_EVENT_IGNORE_MSG;
                break;
            }

            memset(conn->exi_out.dinEXIDocument, 0, sizeof(din_exiDocument));

            v2gEvent = server->handle_request(conn);
            break;

        case V2G_PROTO_ISO15118_2013:
            memset(conn->exi_in.iso2EXIDocument, 0, sizeof(iso2_exiDocument));
            rv = decode_iso2_exiDocument(&conn->stream, conn->exi_in.iso2EXIDocument);
            if (rv != 0) {
                dlog(DLOG_LEVEL_ERROR, "decode_iso2_exiDocument() (previous message \"%s\") failed: %d",
                     v2g_msg_type[conn->ctx->last_v2g_msg], rv);
                /* we must ignore packet which we cannot decode, so reset rv to zero to stay in loop */
                rv = 0;
                v2gEvent = V2G_EVENT_IGNORE_MSG;
                break;
            }
            conn->stream.byte_pos = 0; // Reset pos for the case if exi msg will be configured over mqtt
            memset(conn->exi_out.iso2EXIDocument, 0, sizeof(iso2_exiDocument));

            v2gEvent = server->handle_request(conn);

            break;
        default:
            goto error_out; //     if protocol is unknown
        }

        /* form the content of V2G_Message type and publish the request*/
        if (conn->ctx->debugMode == true) {
            publish_var_V2G_Message(conn, true);
        }

        switch (v2gEvent) {
        case V2G_EVENT_SEND_AND_TERMINATE:
            stop_receiving_loop = true;
        case V2G_EVENT_NO_EVENT: { // fall-through intended
            /* Reset v2g-buffer */
            conn->stream.data[0] = 0;
            conn->stream.bit_count = 0;
            conn->stream.byte_pos = V2GTP_HEADER_LENGTH;
            conn->stream.data_size = DEFAULT_BUFFER_SIZE;

            /* Configure msg and send */
            switch (selected_protocol) {
            case V2G_PROTO_DIN70121:
            case V2G_PROTO_ISO15118_2010:
                if ((rv = encode_din_exiDocument(&conn->stream, conn->exi_out.dinEXIDocument)) != 0) {
                    dlog(DLOG_LEVEL_ERROR, "encode_dinExiDocument() (message \"%s\") failed: %d",
                         v2g_msg_type[conn->ctx->current_v2g_msg], rv);
                }
                break;
            case V2G_PROTO_ISO15118_2013:
                if ((rv = encode_iso2_exiDocument(&conn->stream, conn->exi_out.iso2EXIDocument)) != 0) {
                    dlog(DLOG_LEVEL_ERROR, "encode_iso2_exiDocument() (message \"%s\") failed: %d",
                         v2g_msg_type[conn->ctx->current_v2g_msg], rv);
                }
                break;
            default:
                goto error_out; // protocol is unknown
            }

            /* Wait max. res-time before sending the next response */
            int64_t time_to_conf_res = getmonotonictime() - start_time;

            if (time_to_conf_res < MAX_RES_TIME) {
                // dlog(DLOG_LEVEL_ERROR,"time_to_conf_res %llu", time_to_conf_res);
                std::this_thread::sleep_for(std::chrono::microseconds((MAX_RES_TIME - time_to_conf_res) * 1000));
            } else {
                dlog(DLOG_LEVEL_WARNING, "Response message (type %d) not configured within %d ms (took %" PRIi64 " ms)",
                     conn->ctx->current_v2g_msg, MAX_RES_TIME, time_to_conf_res);
            }
        }
        case V2G_EVENT_SEND_RECV_EXI_MSG: { // fall-through intended
            /* form the content of V2G_Message type and publish the response for debugging*/
            if (conn->ctx->debugMode == true) {
                publish_var_V2G_Message(conn, false);
            }

            /* Write header and send next res-msg */
            if ((rv != 0) or ((rv = v2g_outgoing_v2gtp(conn)) == -1)) {
                dlog(DLOG_LEVEL_ERROR, "v2g_outgoing_v2gtp() \"%s\" failed: %d",
                     v2g_msg_type[conn->ctx->current_v2g_msg], rv);
                break;
            }
            break;
        }
        case V2G_EVENT_IGNORE_MSG:
            dlog(DLOG_LEVEL_ERROR, "Ignoring V2G request message \"%s\". Waiting for next request",
                 v2g_msg_type[conn->ctx->current_v2g_msg]);
            break;
        case V2G_EVENT_TERMINATE_CONNECTION: // fall-through intended
        default:
            dlog(DLOG_LEVEL_ERROR, "Failed to handle V2G request message \"%s\"",
                 v2g_msg_type[conn->ctx->current_v2g_msg]);
            stop_receiving_loop = true;
            break;
        }

    } while ((rv == 0) and (stop_receiving_loop == false));

    update_heartbeat(conn);
#pragma endregion

error_out:

#pragma region Cleanup
    if (test_wrapper != nullptr) {
        // Dispatch a ConnectionCloseEvent
        test_wrapper->dispatch_connection_close_event();
        test_wrapper->dispatch_test_finished();

        std::lock_guard lock(conn->ctx->test_data.test_mutex);
        conn->ctx->test_data.test_wrapper = nullptr;
    }

    switch (selected_protocol) {
    case V2G_PROTO_DIN70121:
    case V2G_PROTO_ISO15118_2010:
        if (conn->exi_in.dinEXIDocument != nullptr)
            free(conn->exi_in.dinEXIDocument);
        if (conn->exi_out.dinEXIDocument != nullptr)
            free(conn->exi_out.dinEXIDocument);
        break;
    case V2G_PROTO_ISO15118_2013:
        if (conn->exi_in.iso2EXIDocument != nullptr)
            free(conn->exi_in.iso2EXIDocument);
        if (conn->exi_out.iso2EXIDocument != nullptr)
            free(conn->exi_out.iso2EXIDocument);
        break;
    default:
        break;
    }

    if (conn->buffer != nullptr) {
        free(conn->buffer);
    }

    // TODO: additional considerations needed
    publish_v2g_test_result(conn);
    publish_v2g_test_status(conn, types::evse_test_common::TestState::Finished);

    v2g_ctx_init_charging_state(conn->ctx, true);
    v2g_ctx_init_testing_values(conn->ctx);
#pragma endregion

    return rv ? -1 : 0;
}

uint64_t v2g_session_id_from_exi(const bool is_iso, void* exi_in) {
    uint64_t session_id = 0;

    if (is_iso) {
        const iso2_exiDocument* req = static_cast<struct iso2_exiDocument*>(exi_in);
        const iso2_MessageHeaderType* hdr = &req->V2G_Message.Header;

        /* the provided session id could be smaller (error) in case that the peer did not
         * send our full session id back to us; this is why we init the id with 0 above
         * and only copy the provided byte len
         */
        memcpy(&session_id, &hdr->SessionID.bytes, std::min(static_cast<int>(sizeof(session_id)),
                                                            static_cast<int>(hdr->SessionID.bytesLen)));
    } else {
        const din_exiDocument* req = static_cast<struct din_exiDocument*>(exi_in);
        const din_MessageHeaderType* hdr = &req->V2G_Message.Header;

        /* see comment above */
        memcpy(&session_id, &hdr->SessionID.bytes, std::min(static_cast<int>(sizeof(session_id)),
                                                            static_cast<int>(hdr->SessionID.bytesLen)));
    }

    return session_id;
}
