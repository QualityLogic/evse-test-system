// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "EvseTestManager.hpp"

#include <everest/logging.hpp>

#include <filesystem>

namespace fs = std::filesystem;

namespace module {

void EvseTestManager::init() {
    invoke_init(*p_evse);

    if (config.remove_session_logs) {
        reset_session_logs();
    }
}

void EvseTestManager::ready() {
    invoke_ready(*p_evse);
}

void EvseTestManager::reset_session_logs() const {
    BOOST_LOG_FUNCTION();

    const auto dir_path = fs::absolute(config.session_logging_path);

    if (!fs::exists(dir_path) or !fs::is_directory(dir_path)) {
        EVLOG_error << "Error resetting session capture files: Path is not a valid directory or does not exist";
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        try {
            fs::remove_all(entry.path()); // recursively remove files and subdirectories
            EVLOG_info << "Removed: " << entry.path();
        } catch (const fs::filesystem_error& e) {
            EVLOG_error << "Error removing " << entry.path() << ": " << e.what();
        }
    }
}

} // namespace module
