// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef TEST_STATE_MACHINE_HPP
#define TEST_STATE_MACHINE_HPP

#include <atomic>
#include <deque>
#include <filesystem>
#include <utility>

#include <generated/types/evse_test_common.hpp>
#include <generated/types/evse_test_manager.hpp>
#include <generated/types/evse_test_v2g.hpp>
#include <sigslot/signal.hpp>

#include "PacketSniffer.hpp"
#include "utils/thread.hpp"
#include "scoped_lock_timeout.hpp"

namespace fs = std::filesystem;

namespace module {

/**
 * A container identifying an individual test instance.
 */
struct test_instance {
    /// A unique identifier for this instance
    std::string instance_id;
    /// A static identifier of the test case
    types::evse_test_common::TestId test_id;
    /// Runtime constraints to apply to the test execution
    types::evse_test_common::TestConstraints constraints;

    test_instance(std::string instance_id,
                  const types::evse_test_common::TestId test_id,
                  types::evse_test_common::TestConstraints constraints) :
        instance_id(std::move(instance_id)), test_id(test_id), constraints(std::move(constraints)) {}
};

/**
 * State enumerations used by the TestStateMachine.
 */
enum class TestingState {
    Disabled,    // Suspend all testing
    Idle,        // Wait for a test to be scheduled
    SetupTest,   // Setup EVSE to run test and wait for EV
    RunningTest, // Wait for test to complete or die (loss of heartbeat)
    CancelTest,  // Halt a running test case
    CleanupTest, // Reset EVSE and connection state
};

/**
 * Convert a testing state into a C-style string.
 *
 * @param state The state to convert to a string.
 * @return A C-style string of the state name.
 */
const char* testing_state_to_string(TestingState state);

class TestStateMachine {
public:
    TestStateMachine(const std::string& device, const fs::path& dir_path_);
    ~TestStateMachine();

    /**
     * Schedule a new test to be run after all previously scheduled tests.
     *
     * @param test_id     Specifies the static ID of the test case.
     * @param constraints Specifies the runtime constraints to apply.
     * @returns A unique ID to identify the scheduled test instance.
     */
    std::string schedule_test(const types::evse_test_common::TestId& test_id,
                              const types::evse_test_common::TestConstraints& constraints);

    /**
     * Unschedule a scheduled (but not running) test instance.
     *
     * @param instance_id The unique ID of the test instance to unschedule.
     */
    void unschedule_test(const std::string& instance_id);

    /**
     * Start running the state machine at regular intervals on a separate thread.
     */
    void run();

    /**
     * Stop the state machine from running at regular intervals.
     */
    void stop();

    /**
     * Set whether the state machine should be enabled.
     *
     * @param enabled Specifies whether the state machine should be enabled.
     */
    void set_enabled(bool enabled);

    void process_heartbeat();
    void process_test_status(const types::evse_test_v2g::TestStatus& status);
    void process_test_result(const types::evse_test_v2g::TestResult& result);

    // signaling
    sigslot::signal<TestingState> signal_state;
    sigslot::signal<test_instance> signal_test_scheduled;
    sigslot::signal<test_instance> signal_test_unscheduled;
    sigslot::signal<test_instance> signal_setup_test;
    sigslot::signal<test_instance> signal_cancel_test;
    sigslot::signal<test_instance> signal_cleanup_test;
    sigslot::signal<types::evse_test_manager::TestStatus> signal_test_status;
    sigslot::signal<types::evse_test_manager::TestResult> signal_test_result;

private:
    /// Locks all variables related to the state machine
    Everest::timed_mutex_traceable state_machine_mutex{};
    /// Main TestStateMachine thread
    Everest::Thread main_thread_handle;
    /// Whether the state machine is allowed to run tests
    std::atomic_bool state_machine_enabled{false};
    /// Queue of test instances awaiting their turn to run
    std::deque<test_instance> scheduled_tests;
    /// Currently selected/running test case
    std::optional<test_instance> current_test;
    /// The point in time the current state started
    std::chrono::system_clock::time_point current_state_started;
    /// The point in time the current test started
    std::chrono::system_clock::time_point current_test_started;
    /// The point in time the last V2G heartbeat was received
    std::chrono::system_clock::time_point last_v2g_heartbeat;
    /// The current state of the state machine
    TestingState current_state{TestingState::Disabled};
    /// The previous state of the state machine
    TestingState last_state{TestingState::Disabled};
    /// An internal state to detect state changes
    TestingState last_state_detect_state_change{TestingState::Disabled};

    // Traffic capture
    PacketSniffer packet_sniffer;
    std::optional<std::string> traffic_filepath;

    /**
     * Invoke run_state_machine at regular intervals until the stop signal
     * is set within main_thread_handle.
     */
    void main_thread();

    /**
     * Run the state machine loop until current_state does not change anymore.
     */
    void run_state_machine();

    void publish_test_status(const test_instance& test, const types::evse_test_common::TestState& state);
    void publish_test_result(const test_instance& test, const types::evse_test_v2g::TestResult& result);

    // constraints
    static constexpr auto MAINLOOP_UPDATE_RATE = std::chrono::milliseconds(250);
    static constexpr auto V2G_HEARTBEAT_TIMEOUT = std::chrono::seconds(30);
    static constexpr auto V2G_CANCELLATION_TIMEOUT = std::chrono::seconds(10);
};

} // namespace module

#endif // TEST_STATE_MACHINE_HPP
