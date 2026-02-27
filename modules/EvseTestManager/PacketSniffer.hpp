// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <pcap.h>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace module {

enum SnifferState {
    SNIFFER_STATE_DISABLED = 0,
    SNIFFER_STATE_STANDBY,
    SNIFFER_STATE_RUNNING,
    SNIFFER_STATE_STOPPING,
    SNIFFER_STATE_STOPPED,
};

class PacketSniffer {
public:
    PacketSniffer(const std::string& device, const fs::path& dir_path_);

    std::optional<std::string> start_capture(const std::string& filename);
    void stop_capture();

private:
    const std::string device;
    const fs::path dir_path;

    bool sniffing_enabled{false};

    std::mutex m;
    std::condition_variable cv;
    std::atomic<SnifferState> state{SNIFFER_STATE_DISABLED};

    pcap_t* p_handle{nullptr};
    pcap_dumper_t* pdumpfile{nullptr};
    char errbuf[PCAP_ERRBUF_SIZE];

    void capture(const std::string& filepath);
};

} // namespace module
