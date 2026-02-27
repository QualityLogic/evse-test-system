// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef EVSE_EVSE_TEST_MANAGER_IMPL_HPP
#define EVSE_EVSE_TEST_MANAGER_IMPL_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 3
//

#include <generated/interfaces/evse_test_manager/Implementation.hpp>

#include "../EvseTestManager.hpp"

// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1
// insert your custom include headers here
#include "TestStateMachine.hpp"
// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1

namespace module {
namespace evse {

struct Conf {};

class evse_test_managerImpl : public evse_test_managerImplBase {
public:
    evse_test_managerImpl() = delete;
    evse_test_managerImpl(Everest::ModuleAdapter* ev, const Everest::PtrContainer<EvseTestManager>& mod, Conf& config) :
        evse_test_managerImplBase(ev, "evse"), mod(mod), config(config){};

    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1
    // insert your public definitions here
    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1

protected:
    // command handler functions (virtual)
    virtual std::string handle_enqueue_test(types::evse_test_common::TestId& test_id,
                                            types::evse_test_common::TestConstraints& constraints) override;
    virtual void handle_cancel_test(std::string& run_id) override;

    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1
    // insert your protected definitions here
    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1

private:
    const Everest::PtrContainer<EvseTestManager>& mod;
    const Conf& config;

    virtual void init() override;
    virtual void ready() override;

    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
    // insert your private definitions here
    std::unique_ptr<TestStateMachine> test_state_machine;
    std::optional<std::string> simulated_ev_connection_payload;
    std::atomic_bool ignore_next_simulated_ev_unplug{false};

    void setup_test(const test_instance& test) const;
    void cancel_test(const test_instance& test) const;
    void cleanup_test(const test_instance& test);

    // EV simulation
    void simulate_ev_plugin() const;
    void simulate_ev_unplug();

    static bool is_own_enable_source(const types::evse_manager::EnableDisableSource& source);
    void enable_evse() const;
    void disable_evse() const;
    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
};

// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1
// insert other definitions here
// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1

} // namespace evse
} // namespace module

#endif // EVSE_EVSE_TEST_MANAGER_IMPL_HPP
