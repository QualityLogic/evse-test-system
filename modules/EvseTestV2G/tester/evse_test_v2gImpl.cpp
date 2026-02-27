// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "evse_test_v2gImpl.hpp"
#include "log.hpp"

using namespace types::evse_test_common;

namespace module::tester {

void evse_test_v2gImpl::init() {
    if (!v2g_ctx) {
        dlog(DLOG_LEVEL_ERROR, "v2g_ctx not created");
        return;
    }
}

void evse_test_v2gImpl::ready() {

    // Some test cases validate how long it takes for an EV to signal a control pilot state change
    mod->r_bsp->subscribe_event([](const types::board_support_common::BspEvent& bsp_event) {
        const auto now = std::chrono::system_clock::now();
        testing::TestWrapper* test_wrapper{nullptr};

        {
            std::lock_guard lock(v2g_ctx->test_data.test_mutex);

            // Update the BSP state
            v2g_ctx->test_data.bsp_state = bsp_event.event;

            // Update the control pilot state
            if (bsp_event.event == types::board_support_common::Event::A or
                bsp_event.event == types::board_support_common::Event::B or
                bsp_event.event == types::board_support_common::Event::C or
                bsp_event.event == types::board_support_common::Event::D or
                bsp_event.event == types::board_support_common::Event::E or
                bsp_event.event == types::board_support_common::Event::F) {

                v2g_ctx->test_data.cp_state = bsp_event.event;
            }

            if (v2g_ctx->test_data.test_wrapper != nullptr)
                test_wrapper = v2g_ctx->test_data.test_wrapper;
        }

        if (test_wrapper != nullptr) {
            test_wrapper->dispatch_update_bsp_event(now, bsp_event);
        }
    });
}

/**
 * @brief Returns a bit mask of V2G application protocols specified by test constraints.
 *
 * If no V2G application protocols are specified by the constraints, the bit mask will
 * contain all currently supported application protocols.
 *
 * @param constraints The test constraints specifying V2G application protocols.
 * @return A bit mask which holds the constrained V2G application protocols.
 */
static int8_t get_v2g_protocol_bitmask(const TestConstraints& constraints) {
    int8_t supported_protocols = 0;

    if (constraints.protocols.has_value() and not constraints.protocols->empty()) {
        for (const auto& protocol : constraints.protocols.value()) {
            switch (protocol) {
            case V2GProtocol::DIN_70121:
                supported_protocols |= (1 << V2G_PROTO_DIN70121);
                break;
            case V2GProtocol::ISO_15118_2:
                supported_protocols |= (1 << V2G_PROTO_ISO15118_2013);
                break;
            default:
                const auto protocol_name = v2gprotocol_to_string(protocol);
                dlog(DLOG_LEVEL_ERROR, "unknown test constraint protocol: %s", protocol_name.c_str());
                break;
            }
        }
    } else {
        // All protocols are supported when none were explicitly specified
        supported_protocols |= v2g_ctx->supported_protocols;
    }

    return supported_protocols;
}

/**
 * @brief Updates the V2G context with identification mode test constraints.
 *
 * If no identification modes are specified by the constraints, the V2G context
 * will indicate that all available identification modes are permitted.
 *
 * @param constraints The test constraints specifying identification modes.
 */
static void apply_identification_modes(const TestConstraints& constraints) {
    auto auth_eim_supported = false;
    auto auth_pnc_supported = false;

    if (constraints.ident_modes.has_value() and not constraints.ident_modes->empty()) {
        for (const auto& identification_mode : constraints.ident_modes.value()) {
            switch (identification_mode) {
            case IdentificationMode::EIM:
                auth_eim_supported = true;
                break;
            case IdentificationMode::PnC:
                auth_pnc_supported = true;
                break;
            default:
                const auto identification_mode_name = identification_mode_to_string(identification_mode);
                dlog(DLOG_LEVEL_ERROR, "unknown test constraint identification mode: %s",
                     identification_mode_name.c_str());
                break;
            }
        }
    } else {
        // All identification modes are supported when none were explicitly specified
        auth_eim_supported = true;
        auth_pnc_supported = true;
    }

    v2g_ctx->test_data.auth_eim_supported = auth_eim_supported;
    v2g_ctx->test_data.auth_pnc_supported = auth_pnc_supported;
}

void evse_test_v2gImpl::handle_set_test_context(TestId& test_id, TestConstraints& constraints) {
    const auto supported_protocols = get_v2g_protocol_bitmask(constraints);

    // TODO(cb): Apply charge mode constraints

    v2g_ctx->test_data.test_id = test_id;
    v2g_ctx->test_data.supported_protocols = supported_protocols;

    apply_identification_modes(constraints);
}

void evse_test_v2gImpl::handle_cancel_test() {
    // your code for cmd cancel_test goes here
}

} // namespace tester::module
