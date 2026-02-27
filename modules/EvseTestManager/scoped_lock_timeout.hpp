// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef SCOPED_LOCK_TIMEOUT
#define SCOPED_LOCK_TIMEOUT

#include "everest/exceptions.hpp"
#include "everest/logging.hpp"
#include <mutex>
#include <signal.h>
#include <thread>

#include "backtrace.hpp"

// Simple helper class for scoped lock with timeout
namespace Everest {

enum class MutexDescription {
    Undefined,
    TSM_subscribe_test,
    TSM_unsubscribe_test,
    TSM_main_thread,
    TSM_process_heartbeat,
    TSM_process_test_status,
    TSM_process_test_result,
};

static std::string to_string(const MutexDescription d) {
    switch (d) {
    case MutexDescription::Undefined:
        return "Undefined";
        return "Orchestrator.cpp: set_evse_ready";
    case MutexDescription::TSM_subscribe_test:
        return "TestStateMachine::subscribe_test";
    case MutexDescription::TSM_unsubscribe_test:
        return "TestStateMachine::unsubscribe_test";
    case MutexDescription::TSM_main_thread:
        return "TestStateMachine::main_thread";
    case MutexDescription::TSM_process_heartbeat:
        return "TestStateMachine::process_heartbeat";
    case MutexDescription::TSM_process_test_status:
        return "TestStateMachine::process_test_status";
    case MutexDescription::TSM_process_test_result:
        return "TestStateMachine::process_test_result";
    }
    return "Undefined";
}

class timed_mutex_traceable : public std::timed_mutex {
#ifdef EVEREST_USE_BACKTRACES
public:
    MutexDescription description;
    pthread_t p_id;
#endif
};

template <typename mutex_type> class scoped_lock_timeout {
public:
    explicit scoped_lock_timeout(mutex_type& __m, MutexDescription description) : mutex(__m) {
        if (not mutex.try_lock_for(deadlock_timeout)) {
#ifdef EVEREST_USE_BACKTRACES
            request_backtrace(pthread_self());
            request_backtrace(mutex.p_id);
            // Give some time for other timeouts to report their state and backtraces
            std::this_thread::sleep_for(std::chrono::seconds(10));

            std::string different_thread;
            if (mutex.p_id not_eq pthread_self()) {
                different_thread = " from a different thread.";
            } else {
                different_thread = " from the same thread";
            }

            EVLOG_AND_THROW(EverestTimeoutError("Mutex deadlock detected: Failed to lock " + to_string(description) +
                                                ", mutex held by " + to_string(mutex.description) + different_thread));
#endif
        } else {
            locked = true;
#ifdef EVEREST_USE_BACKTRACES
            mutex.description = description;
            mutex.p_id = pthread_self();
#endif
        }
    }

    ~scoped_lock_timeout() {
        if (locked) {
            mutex.unlock();
        }
    }

    scoped_lock_timeout(const scoped_lock_timeout&) = delete;
    scoped_lock_timeout& operator=(const scoped_lock_timeout&) = delete;

private:
    bool locked{false};
    mutex_type& mutex;

    // This should be lower than command timeouts from framework (by default 300s)
    static constexpr auto deadlock_timeout = std::chrono::seconds(120);
};

} // namespace Everest

#endif //SCOPED_LOCK_TIMEOUT
