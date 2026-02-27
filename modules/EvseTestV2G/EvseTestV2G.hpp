// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef EVSE_TEST_V2G_HPP
#define EVSE_TEST_V2G_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 2
//

#include "ld-ev.hpp"

// headers for provided interface implementations
#include <generated/interfaces/ISO15118_charger/Implementation.hpp>
#include <generated/interfaces/auth_token_provider/Implementation.hpp>
#include <generated/interfaces/evse_test_v2g/Implementation.hpp>
#include <generated/interfaces/iso15118_extensions/Implementation.hpp>

// headers for required interface implementations
#include <generated/interfaces/evse_board_support/Interface.hpp>
#include <generated/interfaces/evse_security/Interface.hpp>

// ev@4bf81b14-a215-475c-a1d3-0a484ae48918:v1
// insert your custom include headers here
#include "v2g_ctx.hpp"
#include <sigslot/signal.hpp>
#include <tls.hpp>
// ev@4bf81b14-a215-475c-a1d3-0a484ae48918:v1

namespace module {

struct Conf {
    std::string device;
    bool supported_DIN70121;
    bool supported_ISO15118_2;
    std::string tls_security;
    bool terminate_connection_on_failed_response;
    bool tls_key_logging;
    std::string tls_key_logging_path;
    int tls_timeout;
    bool verify_contract_cert_chain;
    int auth_timeout_pnc;
    int auth_timeout_eim;
    bool enable_sdp_server;
    bool include_custom_service;
};

class EvseTestV2G : public Everest::ModuleBase {
public:
    EvseTestV2G() = delete;
    EvseTestV2G(const ModuleInfo& info, Everest::MqttProvider& mqtt_provider,
                std::unique_ptr<ISO15118_chargerImplBase> p_charger,
                std::unique_ptr<iso15118_extensionsImplBase> p_extensions,
                std::unique_ptr<evse_test_v2gImplBase> p_tester,
                std::unique_ptr<auth_token_providerImplBase> p_token_provider,
                std::unique_ptr<evse_board_supportIntf> r_bsp, std::unique_ptr<evse_securityIntf> r_security,
                Conf& config) :
        ModuleBase(info),
        mqtt(mqtt_provider),
        p_charger(std::move(p_charger)),
        p_extensions(std::move(p_extensions)),
        p_tester(std::move(p_tester)),
        p_token_provider(std::move(p_token_provider)),
        r_bsp(std::move(r_bsp)),
        r_security(std::move(r_security)),
        config(config){};

    Everest::MqttProvider& mqtt;
    const std::unique_ptr<ISO15118_chargerImplBase> p_charger;
    const std::unique_ptr<iso15118_extensionsImplBase> p_extensions;
    const std::unique_ptr<evse_test_v2gImplBase> p_tester;
    const std::unique_ptr<auth_token_providerImplBase> p_token_provider;
    const std::unique_ptr<evse_board_supportIntf> r_bsp;
    const std::unique_ptr<evse_securityIntf> r_security;
    const Conf& config;

    // ev@1fce4c5e-0ab8-41bb-90f7-14277703d2ac:v1
    // insert your public definitions here
    ~EvseTestV2G() override;

    sigslot::signal<> signal_new_token;
    // ev@1fce4c5e-0ab8-41bb-90f7-14277703d2ac:v1

protected:
    // ev@4714b2ab-a24f-4b95-ab81-36439e1478de:v1
    // insert your protected definitions here
    // ev@4714b2ab-a24f-4b95-ab81-36439e1478de:v1

private:
    friend class LdEverest;
    void init();
    void ready();

    // ev@211cfdbe-f69a-4cd6-a4ec-f8aaa3d1b6c8:v1
    // insert your private definitions here
    tls::Server tls_server;
    // ev@211cfdbe-f69a-4cd6-a4ec-f8aaa3d1b6c8:v1
};

// ev@087e516b-124c-48df-94fb-109508c7cda9:v1
// insert other definitions here
// ev@087e516b-124c-48df-94fb-109508c7cda9:v1

} // namespace module

#endif // EVSE_TEST_V2G_HPP
