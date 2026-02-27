// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "evse_test_managerImpl.hpp"

#define NODERED_EXECUTE_CHARGING_SESSION_TOPIC(connector_id) std::string("everest_external/nodered/") + std::to_string(connector_id) + "/carsim/cmd/execute_charging_session"
#define NODERED_MODIFY_CHARGING_SESSION_TOPIC(connector_id)  std::string("everest_external/nodered/") + std::to_string(connector_id) + "/carsim/cmd/modify_charging_session"
#define EVSE_ENABLE_DISABLE_PRIORITY 1234

namespace module::evse {

void evse_test_managerImpl::init() {
}

void evse_test_managerImpl::ready() {
    const auto connector_id = mod->config.connector_id;
    test_state_machine = std::make_unique<TestStateMachine>(
        mod->config.capture_device, mod->config.session_logging_path);

    //
    // State machine event handlers
    //

    test_state_machine->signal_test_scheduled.connect([this](const test_instance& test) {
        const auto test_name = types::evse_test_common::test_id_to_string(test.test_id);
        EVLOG_info << "Scheduled test \"" << test_name << "\" (" << test.instance_id << ")";
    });

    test_state_machine->signal_test_unscheduled.connect([](const test_instance& test) {
        const auto test_name = types::evse_test_common::test_id_to_string(test.test_id);
        EVLOG_info << "Unscheduled test \"" << test_name << "\" (" << test.instance_id << ")";
    });

    test_state_machine->signal_setup_test.connect([this](const test_instance& test) {
        const auto test_name = types::evse_test_common::test_id_to_string(test.test_id);
        EVLOG_info << "Setup test \"" << test_name << "\" (" << test.instance_id << ")";

        setup_test(test);
    });

    test_state_machine->signal_cancel_test.connect([this](const test_instance& test) {
        const auto test_name = types::evse_test_common::test_id_to_string(test.test_id);
        EVLOG_info << "Cancel test \"" << test_name << "\" (" << test.instance_id << ")";

        cancel_test(test);
    });

    test_state_machine->signal_cleanup_test.connect([this](const test_instance& test) {
        const auto test_name = types::evse_test_common::test_id_to_string(test.test_id);
        EVLOG_info << "Cleanup test \"" << test_name << "\" (" << test.instance_id << ")";

        cleanup_test(test);
    });

    test_state_machine->signal_test_status.connect([this](const types::evse_test_manager::TestStatus& status) {
        publish_test_status(status);
    });

    test_state_machine->signal_test_result.connect([this](const types::evse_test_manager::TestResult& result) {
        publish_test_result(result);
    });

    //
    // Nodered simulation event handlers
    //

    const auto execute_charge_topic = NODERED_EXECUTE_CHARGING_SESSION_TOPIC(connector_id);
    const auto modify_charge_topic = NODERED_MODIFY_CHARGING_SESSION_TOPIC(connector_id);

    mod->mqtt.subscribe(execute_charge_topic, [this](const std::string& payload) {
        // Is this payload associated with a 'Plug-In' command?
        if (payload.find("iso_wait_slac_matched") != std::string::npos) {
            simulated_ev_connection_payload = payload;
        }
    });

    mod->mqtt.subscribe(modify_charge_topic, [this](const std::string& payload) {
        // Is this payload associated with an 'Unplug' command?
        if (payload.find("unplug") != std::string::npos) {
            if (not ignore_next_simulated_ev_unplug) {
                simulated_ev_connection_payload.reset();
            } else {
                ignore_next_simulated_ev_unplug = false;
            }
        }
    });

    //
    // EVSE Manager event handlers
    //

    mod->r_evse_manager->subscribe_session_event([this](const types::evse_manager::SessionEvent& session_event) {
        switch (session_event.event) {

        case types::evse_manager::SessionEventEnum::Enabled:
            // Was the EVSE enabled by an external source?
            if (not is_own_enable_source(*session_event.source)) {
                disable_evse();
                test_state_machine->set_enabled(true);
            }
            break;

        case types::evse_manager::SessionEventEnum::Disabled:
            // Was the EVSE disabled by an external source?
            if (not is_own_enable_source(*session_event.source)) {
                test_state_machine->set_enabled(false);
            }
            break;

        default:
            break;
        }
    });

    //
    // EVSE V2G event handlers
    //

    mod->r_hlc->subscribe_test_status([this](const types::evse_test_v2g::TestStatus& status) {
        test_state_machine->process_test_status(status);
    });

    mod->r_hlc->subscribe_test_result([this](const types::evse_test_v2g::TestResult& result) {
        test_state_machine->process_test_result(result);
    });

    mod->r_hlc->subscribe_heartbeat([this] {
        // The V2G heartbeat is published approximately every 2s to notify the test is still actively running.
        // This allows the state machine to know if the test hangs or terminates without publishing a test result.
        test_state_machine->process_heartbeat();
    });

    test_state_machine->run();
}

std::string evse_test_managerImpl::handle_enqueue_test(types::evse_test_common::TestId& test_id,
                                                       types::evse_test_common::TestConstraints& constraints) {
    return test_state_machine->schedule_test(test_id, constraints);
}

void evse_test_managerImpl::handle_cancel_test(std::string& run_id) {
    test_state_machine->unschedule_test(run_id);
}

void evse_test_managerImpl::setup_test(const test_instance& test) const {
    // Notify V2G module of the new test configuration
    mod->r_hlc->call_set_test_context(test.test_id, test.constraints);

    enable_evse();

    // Reconnect the simulated EV if applicable
    simulate_ev_plugin();
}

void evse_test_managerImpl::cancel_test(const test_instance& test) const {
    // Notify V2G module of the test cancellation
    mod->r_hlc->call_cancel_test();
}

void evse_test_managerImpl::cleanup_test(const test_instance& test) {
    // Disconnect the simulated EV if applicable
    simulate_ev_unplug();
    disable_evse();
}

#pragma region EV Simulation
void evse_test_managerImpl::simulate_ev_plugin() const {
    if (simulated_ev_connection_payload.has_value()) {
        const auto topic = NODERED_EXECUTE_CHARGING_SESSION_TOPIC(mod->config.connector_id);
        const auto payload = *simulated_ev_connection_payload;
        EVLOG_info << "Simulating EV plug-in";
        mod->mqtt.publish(topic, payload);
    } else {
        EVLOG_debug << "Skipping simulated EV plug-in (connection payload unset)";
    }
}

void evse_test_managerImpl::simulate_ev_unplug() {
    if (simulated_ev_connection_payload.has_value()) {
        const auto topic = NODERED_MODIFY_CHARGING_SESSION_TOPIC(mod->config.connector_id);
        const auto payload = "iso_stop_charging;iso_wait_v2g_session_stopped;unplug";
        EVLOG_info << "Simulating EV unplug";

        // We set this flag to let the MQTT listener know that the next
        // unplug event is not the user manually clicking the button in
        // the browser window.
        ignore_next_simulated_ev_unplug = true;

        mod->mqtt.publish(topic, payload);
    } else {
        EVLOG_debug << "Skipping simulated EV unplug (connection payload unset)";
    }
}
#pragma endregion EV Simulation

#pragma region EVSE Enable/Disable
bool evse_test_managerImpl::is_own_enable_source(const types::evse_manager::EnableDisableSource& source) {
    return (source.enable_source == types::evse_manager::Enable_source::LocalAPI) and
           (source.enable_priority == EVSE_ENABLE_DISABLE_PRIORITY);
}

void evse_test_managerImpl::enable_evse() const {
    mod->r_evse_manager->call_enable_disable(mod->config.connector_id, {
        .enable_source = types::evse_manager::Enable_source::LocalAPI,
        .enable_state = types::evse_manager::Enable_state::Enable,
        .enable_priority = EVSE_ENABLE_DISABLE_PRIORITY,
    });
}

void evse_test_managerImpl::disable_evse() const {
    mod->r_evse_manager->call_enable_disable(mod->config.connector_id, {
        .enable_source = types::evse_manager::Enable_source::LocalAPI,
        .enable_state = types::evse_manager::Enable_state::Disable,
        .enable_priority = EVSE_ENABLE_DISABLE_PRIORITY,
    });
}
#pragma endregion EVSE Enable/Disable

} // namespace module::evse
