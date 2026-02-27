// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef EVSE_TEST_MANAGER_HPP
#define EVSE_TEST_MANAGER_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 2
//

#include "ld-ev.hpp"

// headers for provided interface implementations
#include <generated/interfaces/evse_test_manager/Implementation.hpp>

// headers for required interface implementations
#include <generated/interfaces/evse_manager/Interface.hpp>
#include <generated/interfaces/evse_test_v2g/Interface.hpp>

// ev@4bf81b14-a215-475c-a1d3-0a484ae48918:v1
// insert your custom include headers here
// ev@4bf81b14-a215-475c-a1d3-0a484ae48918:v1

namespace module {

struct Conf {
    int connector_id;
    std::string capture_device;
    std::string session_logging_path;
    bool remove_session_logs;
};

class EvseTestManager : public Everest::ModuleBase {
public:
    EvseTestManager() = delete;
    EvseTestManager(const ModuleInfo& info, Everest::MqttProvider& mqtt_provider,
                    std::unique_ptr<evse_test_managerImplBase> p_evse, std::unique_ptr<evse_test_v2gIntf> r_hlc,
                    std::unique_ptr<evse_managerIntf> r_evse_manager, Conf& config) :
        ModuleBase(info),
        mqtt(mqtt_provider),
        p_evse(std::move(p_evse)),
        r_hlc(std::move(r_hlc)),
        r_evse_manager(std::move(r_evse_manager)),
        config(config){};

    Everest::MqttProvider& mqtt;
    const std::unique_ptr<evse_test_managerImplBase> p_evse;
    const std::unique_ptr<evse_test_v2gIntf> r_hlc;
    const std::unique_ptr<evse_managerIntf> r_evse_manager;
    const Conf& config;

    // ev@1fce4c5e-0ab8-41bb-90f7-14277703d2ac:v1
    // insert your public definitions here
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
    void reset_session_logs() const;
    // ev@211cfdbe-f69a-4cd6-a4ec-f8aaa3d1b6c8:v1
};

// ev@087e516b-124c-48df-94fb-109508c7cda9:v1
// insert other definitions here
// ev@087e516b-124c-48df-94fb-109508c7cda9:v1

} // namespace module

#endif // EVSE_TEST_MANAGER_HPP
