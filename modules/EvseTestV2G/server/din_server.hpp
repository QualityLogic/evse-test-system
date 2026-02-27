// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 chargebyte GmbH
// Copyright (C) 2023 Contributors to EVerest

#ifndef DIN_SERVER_HPP
#define DIN_SERVER_HPP

#include "v2g.hpp"
#include "abc_server.hpp"

/**
 * @brief The din_state_id enum
 */
enum din_state_id {
    WAIT_FOR_SESSIONSETUP = 0,
    WAIT_FOR_SERVICEDISCOVERY,
    WAIT_FOR_PAYMENTSERVICESELECTION,
    WAIT_FOR_AUTHORIZATION,
    WAIT_FOR_CHARGEPARAMETERDISCOVERY,
    WAIT_FOR_CABLECHECK,
    WAIT_FOR_PRECHARGE,
    WAIT_FOR_PRECHARGE_POWERDELIVERY,
    WAIT_FOR_CURRENTDEMAND,
    WAIT_FOR_CURRENTDEMAND_POWERDELIVERY,
    WAIT_FOR_WELDINGDETECTION_SESSIONSTOP,
    WAIT_FOR_SESSIONSTOP,
    WAIT_FOR_TERMINATED_SESSION
};

static const char* dinResponse[] = {"Response OK",
                                    "New Session Established",
                                    "Old Session Joined",
                                    "Certificate Expires Soon",
                                    "Response Failed",
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
                                    "EVSE Present Voltage To Low",
                                    "Metering Signature Not Valid",
                                    "Wrong Energy Transfer Type"};

/**
 * @brief The din_state struct
 */
struct din_state {
    const char* description;
    int allowed_requests;
};

static constexpr din_state din_states[] = {
    /* [V2G-DC-437]  Expected req msg after supportedAppProtocolRes */
    [WAIT_FOR_SESSIONSETUP] = {"Waiting for SessionSetupReq", 1 << V2G_SESSION_SETUP_MSG},
    /* [V2G-DC-439] Expected req msg after SessionSetupRes */
    [WAIT_FOR_SERVICEDISCOVERY] = {"Waiting for ServiceDiscoveryReq, SessionStopReq",
                                   1 << V2G_SERVICE_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-441] Expected req msg after ServiceDiscoveryRes */
    [WAIT_FOR_PAYMENTSERVICESELECTION] = {"Waiting for ServicePaymentSelectionReq, SessionStopReq",
                                          1 << V2G_PAYMENT_SERVICE_SELECTION_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-444] [V2G-DC-497] Expected req msg after ServicePaymentSelectionRes */
    [WAIT_FOR_AUTHORIZATION] = {"Waiting for ContractAuthenticationReq, SessionStopReq",
                                1 << V2G_AUTHORIZATION_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-498], [V2G-DC-495] Expected req msg after ContractAuthenticationRes*/
    [WAIT_FOR_CHARGEPARAMETERDISCOVERY] = {"Waiting for ChargeParameterDiscoveryReq, SessionStopReq",
                                           1 << V2G_CHARGE_PARAMETER_DISCOVERY_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-453], [V2G-DC-499] Expected req msg after ChargeParameterDiscoveryRes, CableCheckRes */
    [WAIT_FOR_CABLECHECK] = {"Waiting for CableCheckReq, SessionStopReq",
                             1 << V2G_CABLE_CHECK_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-455] Expected req msgs after CableCheckRes */
    [WAIT_FOR_PRECHARGE] = {"Waiting for PreChargeReq, SessionStopReq",
                            1 << V2G_PRE_CHARGE_MSG | 1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-458] Expected req msg after PreChargeRes */
    [WAIT_FOR_PRECHARGE_POWERDELIVERY] = {"Waiting for PowerDeliveryReq, PreChargeReq, SessionStopRes",
                                          1 << V2G_POWER_DELIVERY_MSG | 1 << V2G_PRE_CHARGE_MSG |
                                              1 << V2G_SESSION_STOP_MSG},
    /* [V2G-DC-462] Expected req msg after PowerDeliveryRes (Ready to Charge = true) */
    [WAIT_FOR_CURRENTDEMAND] = {"Waiting for CurrentDemandReq", 1 << V2G_CURRENT_DEMAND_MSG},
    /* [V2G-DC-462], [V2G-DC-465] Expected req msg after PowerDeliveryRes (Ready to Charge = true), CurrentDemandRes */
    [WAIT_FOR_CURRENTDEMAND_POWERDELIVERY] = {"Waiting for CurrentDemandReq, PowerDeliveryReq",
                                              1 << V2G_CURRENT_DEMAND_MSG | 1 << V2G_POWER_DELIVERY_MSG},
    /* [V2G-DC-459], [V2G-DC-469] Expected req msg after PowerDeliveryRes, WeldingDetectionRes */
    [WAIT_FOR_WELDINGDETECTION_SESSIONSTOP] = {"Waiting for WeldingDetectionReq, SessionStopReq ",
                                               1 << V2G_WELDING_DETECTION_MSG | 1 << V2G_SESSION_STOP_MSG},
    [WAIT_FOR_SESSIONSTOP] = {"Waiting for SessionStopReq", 1 << V2G_SESSION_STOP_MSG},
    [WAIT_FOR_TERMINATED_SESSION] = {"Terminate session", 0}};

/**
 * @class DinServerBase
 * @brief Virtual class providing a default implementation of the DIN 70121 protocol.
 */
class DinServerBase : virtual public V2GServer {
public:

    v2g_event handle_request(v2g_connection* conn) override;

    V2gMsgTypeId find_req_message_type(const v2g_connection* conn) override;

protected:

    /*!
     * \brief din_validate_state This function checks whether the received message is expected and valid at this
     * point in the communication sequence state machine. The current V2G msg type must be set with the current V2G msg
     * state. \param state is the current state of the charging session \param current_v2g_msg is the current handled V2G
     * message \param state of the actual session. \return Returns a din_ResponseCode with sequence error if current_v2g_msg
     * is not expected, otherwise OK.
     */
    virtual din_responseCodeType din_validate_state(int state, V2gMsgTypeId current_v2g_msg);

    /*!
     * \brief din_validate_response_code This function checks if an external error has occurred (sequence error, user
     * abort)... ). \param din_response_code is a pointer to the current response code. The value will be modified if an
     * external error has occurred. \param conn the structure with the external error information. \return Returns the next
     * v2g-event.
     */
    virtual v2g_event din_validate_response_code(din_responseCodeType* din_response_code, const v2g_connection* conn);

    /*!
     * \brief publish_DIN_DcEvStatus This function is a helper function to publish EVStatusType.
     * \param ctx is a pointer to the V2G context.
     * \param din_ev_status the structure the holds the EV Status elements.
     */
    virtual void publish_DIN_DcEvStatus(v2g_context* ctx, const din_DC_EVStatusType& din_ev_status);

    //=============================================
    //             Request Publishing
    //=============================================

    /*!
     * \brief publish_din_service_discovery_req This function publishes the din_ServiceDiscoveryReqType message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_service_discovery_req is the request message.
     */
    virtual void publish_din_service_discovery_req(v2g_context* ctx,
                                                   const din_ServiceDiscoveryReqType* v2g_service_discovery_req);

    /*!
     * \brief publish_din_service_payment_selection_req This function publishes the din_ServicePaymentSelectionReqType
     * message to the MQTT interface.
     * \param ctx is the V2G context.
     * \param v2g_payment_service_selection_req is the request message.
     */
    virtual void publish_din_service_payment_selection_req(
        v2g_context* ctx, const din_ServicePaymentSelectionReqType* v2g_payment_service_selection_req);

    /*!
     * \brief publish_din_charge_parameter_discovery_req This function publishes the din_ChargeParameterDiscoveryReqType
     * message to the MQTT interface.
     * \param ctx is the V2G context.
     * \param v2g_charge_parameter_discovery_req is the request message.
     */
    virtual void publish_din_charge_parameter_discovery_req(
        v2g_context* ctx, const din_ChargeParameterDiscoveryReqType* v2g_charge_parameter_discovery_req);

    /*!
     * \brief publish_din_power_delivery_req This function publishes the din_PowerDeliveryReqType message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_power_delivery_req is the request message.
     */
    virtual void publish_din_power_delivery_req(v2g_context* ctx,
                                                const din_PowerDeliveryReqType* v2g_power_delivery_req);

    /*!
     * \brief publish_din_precharge_req This function publishes the din_PreChargeReqType message to the MQTT interface.
     * \param ctx is the V2G context.
     * \param v2g_precharge_req is the request message.
     */
    virtual void publish_din_precharge_req(v2g_context* ctx, const din_PreChargeReqType* v2g_precharge_req);

    /*!
     * \brief publish_din_current_demand_req This function publishes the din_CurrentDemandReqType message to the MQTT
     * interface. \param ctx is the V2G context. \param v2g_current_demand_req is the request message.
     */
    virtual void publish_din_current_demand_req(v2g_context* ctx,
                                                const din_CurrentDemandReqType* v2g_current_demand_req);

    //=============================================
    //             Request Handling
    //=============================================

    /*!
     * \brief handle_iso_session_setup This function handles the din_session_setup msg pair. It analyzes the request msg and
     * fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure. [V2G-DC-436]
     * \param conn holds the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_session_setup(v2g_connection* conn);

    /*!
     * \brief handle_din_service_discovery This function handles the din service discovery msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. [V2G-DC-440]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_service_discovery(v2g_connection* conn);

    /*!
     * \brief handle_din_service_payment_selection This function handles the din service payment selection msg pair. It analyzes the
     * request msg and fills the response msg. The request and response msg based on the open V2G structures. This
     * structures must be provided within the \c conn structure. [V2G-DC-443]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_service_payment_selection(v2g_connection* conn);

    /*!
     * \brief handle_din_contract_authentication This function handles the din contract authentication msg pair. It analyzes the
     * request msg and fills the response msg. The request and response msg based on the open V2G structures. This
     * structures must be provided within the \c conn structure. [V2G-DC-494]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_contract_authentication(v2g_connection* conn);

    /*!
     * \brief handle_din_charge_parameter This function handles the din charge parameters msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. [V2G-DC-445]
     * \param conn is the structure with the v2g msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_charge_parameter(v2g_connection* conn);

    /*!
     * \brief handle_din_power_delivery This function handles the din power delivery msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. [V2G-DC-461]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_power_delivery(v2g_connection* conn);

    /*!
     * \brief handle_din_cable_check This function handles the din cable check msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure. [V2G-DC-454]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_cable_check(v2g_connection* conn);

    /*!
     * \brief handle_din_pre_charge This function handles the din pre charge msg pair. It analyzes the request msg
     * and fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure. [V2G-DC-457]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_pre_charge(v2g_connection* conn);

    /*!
     * \brief handle_din_current_demand This function handles the din current demand msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. [V2G-DC-464]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_current_demand(v2g_connection* conn);

    /*!
     * \brief handle_din_welding_detection This function handles the din welding detection msg pair. It analyzes the request
     * msg and fills the response msg. The request and response msg based on the open V2G structures. This structures must
     * be provided within the \c conn structure. [V2G-DC-468]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_welding_detection(v2g_connection* conn);

    /*!
     * \brief handle_din_session_stop This function handles the din session stop msg pair. It analyzes the request msg and
     * fills the response msg. The request and response msg based on the open V2G structures. This structures must be
     * provided within the \c conn structure. [V2G-DC-450]
     * \param conn is the structure with the V2G msg pair.
     * \return Returns the next V2G-event.
     */
    virtual v2g_event handle_din_session_stop(v2g_connection* conn);
};

#endif /* DIN_SERVER_HPP */
