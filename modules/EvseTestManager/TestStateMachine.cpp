// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <fmt/core.h>

#include "TestStateMachine.hpp"

#include "tools.hpp"

namespace module {

static std::string generate_uuid_string() {
    static boost::uuids::random_generator uuid_generator;
    const boost::uuids::uuid uuid = uuid_generator();
    std::stringstream ss;
    ss << uuid;
    return ss.str();
}

TestStateMachine::TestStateMachine(const std::string& device, const fs::path& dir_path_) :
    packet_sniffer(device, std::move(dir_path_)) {
}


TestStateMachine::~TestStateMachine() {
    main_thread_handle.stop();
}

#pragma region scheduling

std::string TestStateMachine::schedule_test(const types::evse_test_common::TestId& test_id,
                                            const types::evse_test_common::TestConstraints& constraints) {
    const auto instance_id = generate_uuid_string();
    test_instance *scheduled_test;

    {
        Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_subscribe_test);
        scheduled_test = &scheduled_tests.emplace_back(instance_id, test_id, constraints);
    }

    signal_test_scheduled(*scheduled_test);
    publish_test_status(*scheduled_test, types::evse_test_common::TestState::Queued);
    return instance_id;
}

void TestStateMachine::unschedule_test(const std::string& instance_id) {
    std::optional<test_instance> unscheduled_test;

    {
        Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_unsubscribe_test);

        // Find the first test instance with a matching instance_id value
        const auto match = std::find_if(scheduled_tests.begin(), scheduled_tests.end(),
            [instance_id](const test_instance& test) {
                return test.instance_id == instance_id;
            });

        // If found, remove the test instance from the queue
        if (match != scheduled_tests.end()) {
            unscheduled_test = *match;
            scheduled_tests.erase(match);
        }
    }

    if (unscheduled_test.has_value()) {
        signal_test_unscheduled(*unscheduled_test);
        publish_test_status(*unscheduled_test, types::evse_test_common::TestState::Canceled);
    }
}

#pragma endregion scheduling

#pragma region lifecycle

void TestStateMachine::run() {
    // spawn new thread and return
    main_thread_handle = std::thread(&TestStateMachine::main_thread, this);
}

void TestStateMachine::stop() {
    main_thread_handle.stop();
}

void TestStateMachine::main_thread() {
    // publish initial values
    signal_state(current_state);

    while (not main_thread_handle.shouldExit()) {
        std::this_thread::sleep_for(MAINLOOP_UPDATE_RATE);

        if (not main_thread_handle.shouldExit()) {
            Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_main_thread);
            // Run our own state machine update (i.e. run everything that needs
            // to be done on regular intervals independent of events)
            run_state_machine();
        }
    }
}

#pragma endregion lifecycle

void TestStateMachine::set_enabled(const bool enabled) {
    state_machine_enabled = enabled;
}

void TestStateMachine::run_state_machine() {
    using std::chrono::duration_cast;
    using std::chrono::system_clock;
    using std::chrono::milliseconds;

    constexpr int max_mainloop_runs = 10;
    int mainloop_runs = 0;

    // run over state machine loop until current_state does not change anymore
    do {
        mainloop_runs++;

        // If a state change happened we reinitialize the state
        const bool initialize_state =
            last_state_detect_state_change not_eq current_state;

        if (initialize_state) {
            EVLOG_info << fmt::format("TestStateMachine state: {}->{}",
                testing_state_to_string(last_state_detect_state_change),
                testing_state_to_string(current_state));
        }

        last_state = last_state_detect_state_change;
        last_state_detect_state_change = current_state;

        const auto now = system_clock::now();

        if (initialize_state) {
            current_state_started = now;
            signal_state(current_state);
        }

        const auto time_in_current_state = duration_cast<milliseconds>(now - current_state_started).count();

        switch (current_state) {

        case TestingState::Disabled:
            // Was the state machine enabled?
            if (state_machine_enabled) {
                current_state = TestingState::Idle;
            }
            break;

        case TestingState::Idle:
            // Was the state machine disabled?
            if (not state_machine_enabled) {
                current_state = TestingState::Disabled;
            }
            // Are any tests scheduled and waiting to run?
            else if (not scheduled_tests.empty()) {
                // Dequeue the test instance at the front of the queue
                current_test = scheduled_tests.front();
                scheduled_tests.pop_front();
                // Update current state
                current_state = TestingState::SetupTest;
            }
            break;

        case TestingState::SetupTest:
            if (initialize_state) {
                signal_setup_test(*current_test);

                EVLOG_info << "Starting packet capture";
                traffic_filepath = packet_sniffer.start_capture(current_test->instance_id);
            }
            break;

        case TestingState::RunningTest:
            if (initialize_state) {
                current_test_started = now;
                last_v2g_heartbeat = now;
            }

            // If the test system is no longer authorized to run tests, we must cancel running
            // the current test case.
            if (not state_machine_enabled) {
                // TODO: Abort the running test and change state
            }

            // If we haven't heard from the V2G module in a while, the test may have hung.
            // To prevent hanging the entire test system forever, we'll abort the test case.
            else if ((now - last_v2g_heartbeat) > V2G_HEARTBEAT_TIMEOUT) {
                EVLOG_error << "V2G module stopped sending heartbeats. Aborting test case";
                current_state = TestingState::CancelTest;
            }

            break;

        case TestingState::CancelTest:
            if (initialize_state) {
                signal_cancel_test(*current_test);
            }

            // The V2G module should quickly report that the test was canceled. If
            // the module does not respond quickly, then it may have hung.
            if (milliseconds(time_in_current_state) > V2G_CANCELLATION_TIMEOUT) {
                EVLOG_error << "V2G module took too long to acknowledge test cancellation.";
                publish_test_status(*current_test, types::evse_test_common::TestState::Canceled);
                current_state = TestingState::CleanupTest;
            }

            break;

        case TestingState::CleanupTest:
            if (initialize_state) {
                signal_cleanup_test(*current_test);

                EVLOG_info << "Stopping packet capture";
                packet_sniffer.stop_capture();
            }

            // TODO: Don't hardcode a wait period
            if (time_in_current_state > milliseconds(10000).count()) {
                current_state = TestingState::Idle;
            }
            break;

        }

        if (mainloop_runs > max_mainloop_runs) {
            EVLOG_warning << "TestStateMachine main loop exceeded maximum number of runs, last_state "
                          << testing_state_to_string(last_state_detect_state_change)
                          << " current_state: " << testing_state_to_string(current_state);
        }

    } while (last_state_detect_state_change not_eq current_state);
}

void TestStateMachine::process_heartbeat() {
    Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_process_heartbeat);
    last_v2g_heartbeat = std::chrono::system_clock::now();
}

void TestStateMachine::process_test_status(const types::evse_test_v2g::TestStatus& status) {
    Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_process_test_status);

    run_state_machine();

    switch (current_state) {
    case TestingState::SetupTest:
        if (status.status == types::evse_test_common::TestState::Running) {
            current_state = TestingState::RunningTest;
            publish_test_status(*current_test, status.status);
        }
        break;
    case TestingState::RunningTest:
    case TestingState::CancelTest:
        if (status.status == types::evse_test_common::TestState::Finished) {
            current_state = TestingState::CleanupTest;
            publish_test_status(*current_test, status.status);
        } else if (status.status == types::evse_test_common::TestState::Canceled) {
            current_state = TestingState::CleanupTest;
            publish_test_status(*current_test, status.status);
        }
        break;
    default:
        break;
    }

    run_state_machine();
}

void TestStateMachine::process_test_result(const types::evse_test_v2g::TestResult& result) {
    Everest::scoped_lock_timeout lock(state_machine_mutex, Everest::MutexDescription::TSM_process_test_result);
    run_state_machine();

    switch (current_state) {
    case TestingState::RunningTest:
    case TestingState::CancelTest:
        publish_test_result(*current_test, result);
        break;
    default:
        break;
    }

    run_state_machine();
}

void TestStateMachine::publish_test_status(const test_instance& test, const types::evse_test_common::TestState& state) {
    const types::evse_test_manager::TestStatus status{
        .run_id = test.instance_id,
        .test_id = test.test_id,
        .status = state,
    };
    signal_test_status(status);
}

void TestStateMachine::publish_test_result(const test_instance& test, const types::evse_test_v2g::TestResult& result) {
    const auto start_timestamp = format_time(current_test_started, ISO_8601_TIMESTAMP);
    const auto end_timestamp = format_time(std::chrono::system_clock::now(), ISO_8601_TIMESTAMP);

    const types::evse_test_common::RunContext context{
        .start_timestamp = start_timestamp,
        .end_timestamp = end_timestamp,
        .protocol = result.context.protocol,
        .charge_mode = result.context.charge_mode,
        .ident_mode = result.context.ident_mode,
    };

    const types::evse_test_manager::TestResult new_result{
        .run_id = test.instance_id,
        .test_id = test.test_id,
        .outcome = result.outcome,
        .context = context,
        .evidence = result.evidence,
        .reports = result.reports,
        .errors = result.errors,
        .pcap_file_path = traffic_filepath,
    };

    signal_test_result(new_result);
}

const char* testing_state_to_string(const TestingState state) {
    switch (state) {
    case TestingState::Disabled:
        return "Disabled";
    case TestingState::Idle:
        return "Idle";
    case TestingState::SetupTest:
        return "SetupTest";
    case TestingState::RunningTest:
        return "RunningTest";
    case TestingState::CancelTest:
        return "CancelTest";
    case TestingState::CleanupTest:
        return "CleanupTest";
    default:
        return "Unknown";
    }
}

} // namespace module
