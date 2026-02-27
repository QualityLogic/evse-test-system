// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "tools.hpp"

[[nodiscard]] std::string format_time(const std::chrono::system_clock::time_point& time, const char* format) {
    char buffer[100];
    const std::time_t now_c = std::chrono::system_clock::to_time_t(time);
    const std::tm* tm_struct = std::gmtime(&now_c); // UTC
    std::strftime(buffer, sizeof(buffer), format, tm_struct);
    return std::string(buffer);
}
