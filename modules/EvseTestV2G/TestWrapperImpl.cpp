// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "TestWrapperImpl.hpp"

namespace testing {

TestWrapperImpl::TestWrapperImpl(v2g_connection* conn, Test* test) :
    connection(conn), test(test) {}

void TestWrapperImpl::dispatch_test_started() {
    test->on_test_started(connection);
}

void TestWrapperImpl::dispatch_test_finished() {
    test->on_test_finished(connection);
}

void TestWrapperImpl::dispatch_connection_close_event() {
    const auto now = std::chrono::system_clock::now();
    dispatch_connection_close_event(now);
}

void TestWrapperImpl::dispatch_connection_close_event(const std::chrono::system_clock::time_point& tp) {
    const ConnectionCloseEvent event{tp};
    test->on_connection_close_event(connection, event);
}

void TestWrapperImpl::dispatch_update_bsp_event(const types::board_support_common::BspEvent& bsp_event) {
    const auto now = std::chrono::system_clock::now();
    dispatch_update_bsp_event(now, bsp_event);
}

void TestWrapperImpl::dispatch_update_bsp_event(const std::chrono::system_clock::time_point& tp,
                                                const types::board_support_common::BspEvent& bsp_event) {
    const UpdateBspEvent event{tp, bsp_event.event};
    test->on_update_bsp_event(connection, event);
}

void TestWrapperImpl::dispatch_powermeter_event(const types::powermeter::Powermeter& powermeter) {
    const auto now = std::chrono::system_clock::now();
    dispatch_powermeter_event(now, powermeter);
}

void TestWrapperImpl::dispatch_powermeter_event(const std::chrono::system_clock::time_point& tp,
                                                const types::powermeter::Powermeter& powermeter) {
    const PowermeterEvent event{tp, powermeter};
    test->on_powermeter_event(connection, event);
}


} // namespace testing
