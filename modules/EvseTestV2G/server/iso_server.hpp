// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 chargebyte GmbH
// Copyright (C) 2023 Contributors to EVerest

#ifndef ISO_SERVER_HPP
#define ISO_SERVER_HPP

#include "v2g.hpp"
#include "abc_server.hpp"

struct iso_state {
    const char* description;
    int allowed_requests;
};

enum class iso_ac_state_id {
    WAIT_FOR_SESSIONSETUP = 0,
    WAIT_FOR_SERVICEDISCOVERY,
    WAIT_FOR_SVCDETAIL_PAYMENTSVCSEL,
    WAIT_FOR_PAYMENTDETAILS_CERTINST_CERTUPD,
    WAIT_FOR_PAYMENTDETAILS,
    WAIT_FOR_AUTHORIZATION,
    WAIT_FOR_CHARGEPARAMETERDISCOVERY,
    WAIT_FOR_POWERDELIVERY,
    WAIT_FOR_CHARGINGSTATUS,
    WAIT_FOR_CHARGINGSTATUS_POWERDELIVERY,
    WAIT_FOR_METERINGRECEIPT,
    WAIT_FOR_SESSIONSTOP,
    WAIT_FOR_TERMINATED_SESSION
};

enum class iso_dc_state_id {
    WAIT_FOR_SESSIONSETUP = 0,
    WAIT_FOR_SERVICEDISCOVERY,
    WAIT_FOR_SVCDETAIL_PAYMENTSVCSEL,
    WAIT_FOR_PAYMENTDETAILS_CERTINST_CERTUPD,
    WAIT_FOR_PAYMENTDETAILS,
    WAIT_FOR_AUTHORIZATION,
    WAIT_FOR_CHARGEPARAMETERDISCOVERY,
    WAIT_FOR_CABLECHECK,
    WAIT_FOR_PRECHARGE,
    WAIT_FOR_PRECHARGE_POWERDELIVERY,
    WAIT_FOR_CURRENTDEMAND_POWERDELIVERY,
    WAIT_FOR_CURRENTDEMAND,
    WAIT_FOR_METERINGRECEIPT,
    WAIT_FOR_WELDINGDETECTION_SESSIONSTOP,
    WAIT_FOR_TERMINATED_SESSION
};

static const char* isoResponse[] = {
    "Response OK",
    "New Session Established",
    "Old Session Joined",
    "Certificate Expires Soon",
    "Response FAILED",
    "Sequence Error",
    "Service ID Invalid",
    "Unknown Session",
    "Service Selection Invalid",
    "Payment Selection Invalid",
    "Certificate Expired",
    "Signature Error",
    "No Certificate Available",
    "Cert Chain Error",
    "Challenge Invalid",
    "Contract Canceled",
    "Wrong Charge Parameter",
    "Power Delivery Not Applied",
    "Tariff Selection Invalid",
    "Charging Profile Invalid",
    "Metering Signature Not Valid",
    "No Charge Service Selected",
    "Wrong Energy Transfer Mode",
    "Contactor Error",
    "Certificate Not Allowed At This EVSE",
    "Certificate Revoked",
};

static const iso_state iso_ac_states[] = {
    {"Waiting for SessionSetupReq", 1 << V2G_SESSION_SETUP_MSG},
    /* [V2G-543] Expected req msg after SessionSetupRes */
    {"Waiting for ServiceDiscoveryReq, SessionStopReq", 1 << V2G_SERVICE_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-545] Expected req msg after ServiceDiscoveryRes */
    {"Waiting for ServiceDetailReq, PaymentServiceSelectionReq, SessionStopReq",
     1 << V2G_SERVICE_DETAIL_MSG | 1 << V2G_PAYMENT_SERVICE_SELECTION_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-551] Expected req msg after ServicePaymentSelectionRes */
    {"Waiting for PaymentDetailsReq, CertificateInstallationReq, CertificateUpdateReq, SessionStopReq",
     1 << V2G_PAYMENT_DETAILS_MSG | 1 << V2G_CERTIFICATE_INSTALLATION_MSG | 1 << V2G_CERTIFICATE_UPDATE_MSG |
         1 << V2G_SESSION_STOP_MSG},
    {"Waiting for PaymentDetailsReq, SessionStopReq", 1 << V2G_PAYMENT_DETAILS_MSG | 1 << V2G_SESSION_STOP_MSG},
    {"Waiting for AuthorizationReq, SessionStopReq", 1 << V2G_AUTHORIZATION_MSG | 1 << V2G_SESSION_STOP_MSG},
    {"Waiting for ChargeParameterDiscoveryReq, SessionStopReq",
     1 << V2G_CHARGE_PARAMETER_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    {"Waiting for PowerDeliveryReq, SessionStopReq", 1 << V2G_POWER_DELIVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    {"Waiting for ChargingStatusReq", 1 << V2G_CHARGING_STATUS_MSG},
    {"Waiting for ChargingStatusReq, PowerDeliveryReq", 1 << V2G_CHARGING_STATUS_MSG | 1 << V2G_POWER_DELIVERY_MSG},
    {"Waiting for MeteringReceiptReq", 1 << V2G_METERING_RECEIPT_MSG},
    {"Waiting for SessionStopReq", 1 << V2G_SESSION_STOP_MSG},
    {"Closing session", 0}};

static const iso_state iso_dc_states[] = {
    /* [V2G-541] Expected req msg after SupportedAppProtocolRes */
    {"Waiting for SessionSetupReq", 1 << V2G_SESSION_SETUP_MSG},
    /* [V2G-543] Expected req msg after SessionSetupRes */
    {"Waiting for ServiceDiscoveryReq, SessionStopReq", 1 << V2G_SERVICE_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-545] Expected req msg after ServiceDiscoveryRes */
    {"Waiting for ServiceDetailReq, ServicePaymentSelectionReq, SessionStopReq",
     1 << V2G_SERVICE_DETAIL_MSG | 1 << V2G_PAYMENT_SERVICE_SELECTION_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-551] Expected req msg after ServicePaymentSelectionRes */
    {"Waiting for PaymentDetailsReq, AuthorizationReq, CertificateInstallationReq, CertificateUpdateReq, "
     "SessionStopReq",
     1 << V2G_PAYMENT_DETAILS_MSG | 1 << V2G_AUTHORIZATION_MSG | 1 << V2G_CERTIFICATE_INSTALLATION_MSG |
         1 << V2G_CERTIFICATE_UPDATE_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-557], [V2G-554], [V2G-558] Expected req msg after CertificateInstallationRes or CertificateUpdateRes */
    {"Waiting for PaymentDetailsReq, SessionStopReq", 1 << V2G_PAYMENT_DETAILS_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-560], [V2G-687] Expected req msg after PaymentDetailsRes, ContractAuthenticationRes */
    {"Waiting for AuthorizationReq, SessionStopReq", 1 << V2G_AUTHORIZATION_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-573], [V2G-813],[V2G-688] Expected req msg after AuthorizationRes or PowerDeliveryRes or
       ChargeParameterDiscoveryRes */
    {"Waiting for ChargeParameterDiscoveryReq, SessionStopReq",
     1 << V2G_CHARGE_PARAMETER_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-582], [V2G-621] Expected req msg after CableCheckRes or ChargeParameterDiscoveryRes */
    {"Waiting for CableCheckReq, SessionStopReq", 1 << V2G_CABLE_CHECK_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-584] Expected req msg after CableCheckRes */
    {"Waiting for PreChargeReq, SessionStopReq", 1 << V2G_PRE_CHARGE_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-587] Expected req msg after PreChargeRes */
    {"Waiting for PreChargeReq, PowerDeliveryReq, SessionStopReq",
     1 << V2G_PRE_CHARGE_MSG | 1 << V2G_POWER_DELIVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-797] Expected req msg after CurrentDemandRes or MeteringReceiptRes*/
    {"Waiting for CurrentDemandReq, PowerDeliveryReq", 1 << V2G_CURRENT_DEMAND_MSG | 1 << V2G_POWER_DELIVERY_MSG},
    /* [V2G-590] Expected req msg after PowerDeliveryRes or CurrentDemandRes or MeteringReceiptRes*/
    {"Waiting for CurrentDemandReq", 1 << V2G_CURRENT_DEMAND_MSG},
    /* [V2G-795] Expected req msg after CurrentDemandRes */
    {"Waiting for MeteringReceiptReq", 1 << V2G_METERING_RECEIPT_MSG},
    /* [V2G-597], [V2G-601] Expected req msg after PowerDeliveryRes or WeldingDetectionRes*/
    {"Waiting for WeldingDetectionReq, SessionStopReq", 1 << V2G_WELDING_DETECTION_MSG | 1 << V2G_SESSION_STOP_MSG},
    {"Closing session", 0}};

/**
 * @class IsoServerBase
 * @brief Virtual class providing a default implementation of the ISO 15118 protocol.
 */
class IsoServerBase : virtual public V2GServer {
public:

    v2g_event handle_request(v2g_connection* conn) override;

    V2gMsgTypeId find_req_message_type(const v2g_connection* conn) override;

protected:

    /*!
     * \brief iso_validate_state This function checks whether the received message is expected and valid at this
     * point in the communication sequence state machine. The current V2G msg type must be set with the current V2G msg
     * state. [V2G2-538]
     * \param state is the current state of the charging session.
     * \param current_v2g_msg is the current handled V2G message.
     * \param is_dc_charging is \c true if it is a DC charging session.
     * \return Returns a iso2_responseCode with sequence error if current_v2g_msg is not expected, otherwise OK.
     */
    virtual iso2_responseCodeType iso_validate_state(int state, V2gMsgTypeId current_v2g_msg, bool is_dc_charging);

    /*!
     * \brief iso_validate_response_code This function checks if an external error has occurred (sequence error, user abort)
     * ... ). \param v2g_response_code is a pointer to the current response code. The value will be modified if an external
     *  error has occurred.
     * \param conn the structure with the external error information.
     * \return Returns \c V2G_EVENT_SEND_AND_TERMINATE if the charging must be terminated after sending the response
     * message, returns \c V2G_EVENT_TERMINATE_CONNECTION if charging must be aborted immediately and \c V2G_EVENT_NO_EVENT
     * if no error
     */
    virtual v2g_event iso_validate_response_code(iso2_responseCodeType* v2g_response_code, const v2g_connection* conn);

    /*!
     * \brief populate_ac_evse_status This function configures the evse_status struct
     * \param ctx is the V2G context
     * \param evse_status is the destination struct
     */
    virtual void populate_ac_evse_status(v2g_context* ctx, iso2_AC_EVSEStatusType* evse_status);

    /*!
     * \brief check_iso2_charging_profile_values This function checks if EV charging profile values are within permissible
     * ranges \param req is the PowerDeliveryReq \param res is the PowerDeliveryRes \param conn holds the structure with the
     * V2G msg pair \param sa_schedule_tuple_idx is the index of SA schedule tuple
     */
    virtual void check_iso2_charging_profile_values(const iso2_PowerDeliveryReqType* req,
                                                    const iso2_PowerDeliveryResType* res,
                                                    const v2g_connection* conn,
                                                    uint8_t sa_schedule_tuple_idx);

    virtual void publish_DcEvStatus(v2g_context* ctx, const iso2_DC_EVStatusType& iso2_ev_status);

    virtual iso2_DC_EVSEStatusCodeType get_emergency_status_code(const v2g_context* ctx, uint8_t phase_type);

    //=============================================
    //             Request Publishing
    //=============================================

    /*!
     * \brief publish_iso_service_discovery_req This function publishes the iso_service_discovery_req message to the MQTT
     * interface. \param v2g_service_discovery_req is the request message.
     */
    virtual void publish_iso_service_discovery_req(const iso2_ServiceDiscoveryReqType* v2g_service_discovery_req);

    /*!
     * \brief publish_iso_service_detail_req This function publishes the iso_service_detail_req message to the MQTT
     * interface. \param v2g_service_detail_req is the request message.
     */
    virtual void publish_iso_service_detail_req(const iso2_ServiceDetailReqType* v2g_service_detail_req);

    /*!
     * \brief publish_iso_payment_service_selection_req This function publishes the iso_payment_service_selection_req
     * message to the MQTT interface.
     * \param v2g_payment_service_selection_req is the request message.
     */
    virtual void publish_iso_payment_service_selection_req(
        const iso2_PaymentServiceSelectionReqType* v2g_payment_service_selection_req);

    /*!
     * \brief publish_iso_authorization_req This function publishes the publish_iso_authorization_req message to the MQTT
     * interface. \param v2g_authorization_req is the request message.
     */
    virtual void publish_iso_authorization_req(const iso2_AuthorizationReqType* v2g_authorization_req);

    /*!
     * \brief publish_iso_charge_parameter_discovery_req This function publishes the charge_parameter_discovery_req message
     * to the MQTT interface. \param ctx is the V2G context. \param v2g_charge_parameter_discovery_req is the request
     * message.
     */
    virtual void publish_iso_charge_parameter_discovery_req(
        v2g_context* ctx, const iso2_ChargeParameterDiscoveryReqType* v2g_charge_parameter_discovery_req);

    /*!
     * \brief publish_iso_pre_charge_req This function publishes the iso_pre_charge_req message to the MQTT interface.
     * \param ctx is the V2G context.
     * \param v2g_precharge_req is the request message.
     */
    virtual void publish_iso_pre_charge_req(v2g_context* ctx, const iso2_PreChargeReqType* v2g_precharge_req);

    /*!
     * \brief publish_iso_power_delivery_req This function publishes the iso_power_delivery_req message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_power_delivery_req is the request message.
     */
    virtual void publish_iso_power_delivery_req(v2g_context* ctx,
                                                const iso2_PowerDeliveryReqType* v2g_power_delivery_req);

    /*!
     * \brief publish_iso_current_demand_req This function publishes the iso_current_demand_req message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_current_demand_req is the request message.
     */
    virtual void publish_iso_current_demand_req(v2g_context* ctx,
                                                const iso2_CurrentDemandReqType* v2g_current_demand_req);

    /*!
     * \brief publish_iso_metering_receipt_req This function publishes the iso_metering_receipt_req message to the MQTT
     * interface. \param v2g_metering_receipt_req is the request message.
     */
    virtual void publish_iso_metering_receipt_req(const iso2_MeteringReceiptReqType* v2g_metering_receipt_req);

    /*!
     * \brief publish_iso_welding_detection_req This function publishes the iso_welding_detection_req message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_welding_detection_req is the request message.
     */
    virtual void publish_iso_welding_detection_req(v2g_context* ctx,
                                                   const iso2_WeldingDetectionReqType* v2g_welding_detection_req);

    /*!
     * \brief publish_iso_certificate_installation_exi_req This function publishes the iso_certificate_update_req message to
     * the MQTT interface.
     * \param ctx is the V2G context.
     * \param AExiBuffer is the exi msg where the V2G EXI msg is stored.
     * \param AExiBufferSize is the size of the V2G msg.
     * \return Returns \c true if it was successful, otherwise \c false.
     */
    virtual bool publish_iso_certificate_installation_exi_req(const v2g_context* ctx,
                                                              const uint8_t* AExiBuffer,
                                                              size_t AExiBufferSize);

    //=============================================
    //             Request Handling
    //=============================================

    /*!
     * \brief handle_iso_session_setup This function handles the iso_session_setup msg pair. It analyzes the request msg and
     * fills the response msg. \param conn holds the structure with the V2G msg pair. \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_session_setup(v2g_connection* conn);

    /*!
     * \brief handle_iso_service_discovery This function handles the din service discovery msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_service_discovery(v2g_connection* conn);

    /*!
     * \brief handle_iso_service_detail This function handles the iso_service_detail msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure. (Optional VAS)
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_service_detail(v2g_connection* conn);

    /*!
     * \brief handle_iso_payment_service_selection This function handles the iso_payment_service_selection msg pair. It
     * analyzes the request msg and fills the response msg. The request and response msg based on the open V2G structures.
     * This structures must be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_payment_service_selection(v2g_connection* conn);

    /*!
     * \brief handle_iso_payment_details This function handles the iso_payment_details msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_payment_details(v2g_connection* conn);

    /*!
     * \brief handle_iso_authorization This function handles the iso_authorization msg pair. It analyzes the request msg and
     * fills the response msg. The request and response msg based on the open v2g structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the v2g msg pair.
     * \return Returns the next v2g-event.
     */
    virtual v2g_event handle_iso_authorization(v2g_connection* conn);

    /*!
     * \brief handle_iso_charge_parameter_discovery This function handles the iso_charge_parameter_discovery msg pair. It
     * analyzes the request msg and fills the response msg. The request and response msg based on the open V2G structures.
     * This structures must be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_charge_parameter_discovery(v2g_connection* conn);

    /*!
     * \brief handle_iso_power_delivery This function handles the iso_power_delivery msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_power_delivery(v2g_connection* conn);

    /*!
     * \brief handle_iso_charging_status This function handles the iso_charging_status msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_charging_status(v2g_connection* conn);

    /*!
     * \brief handle_iso_metering_receipt This function handles the iso_metering_receipt msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. \param conn holds the structure with the V2G msg pair. \return Returns the
     * next V2G-event.
     */
    virtual v2g_event handle_iso_metering_receipt(v2g_connection* conn);

    /*!
     * \brief handle_iso_certificate_update This function handles the iso_certificate_update msg pair. It analyzes the
     * request msg and fills the response msg. The request and response msg based on the open V2G structures. This
     * structures must be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_certificate_update(v2g_connection* conn);

    /*!
     * \brief handle_iso_certificate_installation This function handles the iso_certificate_installation msg pair. It
     * analyzes the request msg and fills the response msg. The request and response msg based on the open V2G structures.
     * This structures must be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_certificate_installation(v2g_connection* conn);

    /*!
     * \brief handle_iso_cable_check This function handles the iso_cable_check msg pair. It analyzes the request msg and
     * fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_cable_check(v2g_connection* conn);

    /*!
     * \brief handle_iso_pre_charge This function handles the iso_pre_charge msg pair. It analyzes the request msg and fills
     * the response msg. The request and response msg based on the open V2G structures. This structures must be provided
     * within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_pre_charge(v2g_connection* conn);

    /*!
     * \brief handle_iso_current_demand This function handles the iso_current_demand msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_current_demand(v2g_connection* conn);

    /*!
     * \brief handle_iso_welding_detection This function handles the iso_welding_detection msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_welding_detection(v2g_connection* conn);

    /*!
     * \brief handle_iso_session_stop This function handles the iso_session_stop msg pair. It analyses the request msg and
     * fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure.
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_iso_session_stop(v2g_connection* conn);
};

#endif /* ISO_SERVER_HPP */
