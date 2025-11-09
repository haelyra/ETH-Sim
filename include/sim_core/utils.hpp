#pragma once

#include <chrono>
#include <cstdint>

namespace sim_core {

inline uint64_t current_time_ms() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto duration = now.time_since_epoch();
    return duration_cast<milliseconds>(duration).count();
}

inline std::pair<std::string, uint16_t> parse_bind_address(const std::string& bind_addr) {
    auto colon_pos = bind_addr.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid bind address format: " + bind_addr);
    }

    std::string host = bind_addr.substr(0, colon_pos);
    uint16_t port = std::stoi(bind_addr.substr(colon_pos + 1));

    return {host, port};
}

}
