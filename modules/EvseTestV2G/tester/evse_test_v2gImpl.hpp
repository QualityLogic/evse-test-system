// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef TESTER_EVSE_TEST_V2G_IMPL_HPP
#define TESTER_EVSE_TEST_V2G_IMPL_HPP

//
// AUTO GENERATED - MARKED REGIONS WILL BE KEPT
// template version 3
//

#include <generated/interfaces/evse_test_v2g/Implementation.hpp>

#include "../EvseTestV2G.hpp"

// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1
// insert your custom include headers here
#include "v2g.hpp"
extern struct v2g_context* v2g_ctx;
// ev@75ac1216-19eb-4182-a85c-820f1fc2c091:v1

namespace module {
namespace tester {

struct Conf {};

class evse_test_v2gImpl : public evse_test_v2gImplBase {
public:
    evse_test_v2gImpl() = delete;
    evse_test_v2gImpl(Everest::ModuleAdapter* ev, const Everest::PtrContainer<EvseTestV2G>& mod, Conf& config) :
        evse_test_v2gImplBase(ev, "tester"), mod(mod), config(config){};

    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1
    // insert your public definitions here
    // ev@8ea32d28-373f-4c90-ae5e-b4fcc74e2a61:v1

protected:
    // command handler functions (virtual)
    virtual void handle_set_test_context(types::evse_test_common::TestId& test_id,
                                         types::evse_test_common::TestConstraints& constraints) override;
    virtual void handle_cancel_test() override;

    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1
    // insert your protected definitions here
    // ev@d2d1847a-7b88-41dd-ad07-92785f06f5c4:v1

private:
    const Everest::PtrContainer<EvseTestV2G>& mod;
    const Conf& config;

    virtual void init() override;
    virtual void ready() override;

    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
    // insert your private definitions here
    // ev@3370e4dd-95f4-47a9-aaec-ea76f34a66c9:v1
};

// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1
// insert other definitions here
// ev@3d7da0ad-02c2-493d-9920-0bbbd56b9876:v1

} // namespace tester
} // namespace module

#endif // TESTER_EVSE_TEST_V2G_IMPL_HPP
