// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "PacketSniffer.hpp"

#include <chrono>
#include <everest/logging.hpp>
#include <fmt/core.h>
#include <pcap.h>
#include <thread>

namespace module {

constexpr bool PROMISC_MODE = true;
constexpr int PACKET_BUFFER_TIMEOUT_MS = 1000;
constexpr int ALL_PACKETS_PROCESSED = -1;
constexpr int WAIT_FOR_MS = 10;
constexpr int BUFFER_SIZE = 8192;

PacketSniffer::PacketSniffer(const std::string& device, const fs::path& dir_path_) :
    device(device), dir_path(fs::absolute(dir_path_)) {
    BOOST_LOG_FUNCTION();
    p_handle = pcap_open_live(device.c_str(), BUFFER_SIZE, PROMISC_MODE, PACKET_BUFFER_TIMEOUT_MS, errbuf);

    if (p_handle == nullptr) {
        std::string error_message(errbuf);
        EVLOG_error << fmt::format("Could not open device {}: {}. Sniffing disabled.", device, error_message);
        return;
    }

    // if (pcap_datalink(p_handle) != DLT_EN10MB) {
    //     EVLOG_error << fmt::format("Device {} doesn't provide Ethernet headers - not supported. Sniffing disabled.",
    //                                device);
    //     return;
    // }

    if (!fs::exists(dir_path)) {
        try {
            fs::create_directories(dir_path);
        } catch (const fs::filesystem_error& e) {
            EVLOG_error << "Error creating " << dir_path << ": " << e.what();
        }
    }

    if (!fs::exists(dir_path) or !fs::is_directory(dir_path)) {
        EVLOG_error << "Session capture log path is not a valid directory or does not exist";
        sniffing_enabled = false;
    } else {
        sniffing_enabled = true;
        state.store(SNIFFER_STATE_STANDBY);
    }
}

std::optional<std::string> PacketSniffer::start_capture(const std::string& filename) {
    std::optional<std::string> filepath;
    BOOST_LOG_FUNCTION();
    if (!sniffing_enabled) {
        EVLOG_warning << "Capturing disabled. Ignoring this StartCapture request.";
        return filepath;
    }

    auto standby_state = SNIFFER_STATE_STANDBY;

    if (state.compare_exchange_strong(standby_state, SNIFFER_STATE_RUNNING)) {
        filepath = (dir_path / (filename + ".dump")).string();
        std::thread(&PacketSniffer::capture, this, filepath.value()).detach();
    } else {
        EVLOG_warning << "Capturing already started. Ignoring this StartCapture request.";
    }

    return filepath;
}

void PacketSniffer::stop_capture() {
    if (!sniffing_enabled)
        return;

    auto running_state = SNIFFER_STATE_RUNNING;
    auto stopped_state = SNIFFER_STATE_STOPPED;

    // If the capture is running, tell the capture thread to stop.
    if (state.compare_exchange_strong(running_state, SNIFFER_STATE_STOPPING)) {
        // Success!
        // pcap_breakloop(p_handle);
    }

    // Wait for the capture thread to acknowledge the request to stop.
    if (state.load() == SNIFFER_STATE_STOPPING) {
        std::unique_lock lock(m);
        cv.wait(lock, [&]{ return state.load() != SNIFFER_STATE_STOPPING; });
    }

    // Reset the sniffer state back to STANDBY
    if (state.compare_exchange_strong(stopped_state, SNIFFER_STATE_STANDBY)) {
        // Success!
    }
}

void PacketSniffer::capture(const std::string& filepath) {
    BOOST_LOG_FUNCTION();
    EVLOG_info << "Start capturing";

    auto running_state = SNIFFER_STATE_RUNNING;
    auto stopping_state = SNIFFER_STATE_STOPPING;

    if ((pdumpfile = pcap_dump_open(p_handle, filepath.c_str())) == nullptr) {
        EVLOG_error << fmt::format("Error opening savefile {} for writing: {}", filepath, pcap_geterr(p_handle));
        return;
    }

    while (state.load() == SNIFFER_STATE_RUNNING) {
        if (pcap_dispatch(p_handle, ALL_PACKETS_PROCESSED, &pcap_dump, reinterpret_cast<u_char*>(pdumpfile)) <= PCAP_ERROR) {
            if (state.compare_exchange_strong(running_state, SNIFFER_STATE_STOPPING)) {
                EVLOG_error << fmt::format("Error reading packets from interface: {}, error: {}",
                                           device, pcap_geterr(p_handle));
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_FOR_MS));
    }

    pcap_dump_close(pdumpfile);
    EVLOG_info << "Capturing stopped.";

    // Notify any threads waiting for the packet capture to stop.
    if (state.compare_exchange_strong(stopping_state, SNIFFER_STATE_STOPPED)) {
        cv.notify_all();
    }
}

} // namespace module
