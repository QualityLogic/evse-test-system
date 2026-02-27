// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <chrono>
#include <string>

#define ISO_8601_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ"

[[nodiscard]] std::string format_time(const std::chrono::system_clock::time_point& time, const char* format);

#endif //TOOLS_HPP
