#pragma once

#include <atomic>
#include <string>
#include <sstream>

namespace sim_core {

class Metrics {
public:
    std::atomic<uint64_t> price_ticks_generated{0};
    std::atomic<uint64_t> ws_frames_sent{0};
    std::atomic<uint64_t> ws_frames_dropped{0};
    std::atomic<uint64_t> ws_frames_duplicated{0};

    void reset() {
        price_ticks_generated = 0;
        ws_frames_sent = 0;
        ws_frames_dropped = 0;
        ws_frames_duplicated = 0;
    }

    std::string to_prometheus() const {
        std::ostringstream oss;

        oss << "# HELP price_ticks_generated Total price ticks generated\n";
        oss << "# TYPE price_ticks_generated counter\n";
        oss << "price_ticks_generated " << price_ticks_generated.load() << "\n\n";

        oss << "# HELP ws_frames_sent Total WebSocket frames sent\n";
        oss << "# TYPE ws_frames_sent counter\n";
        oss << "ws_frames_sent " << ws_frames_sent.load() << "\n\n";

        oss << "# HELP ws_frames_dropped Total WebSocket frames dropped\n";
        oss << "# TYPE ws_frames_dropped counter\n";
        oss << "ws_frames_dropped " << ws_frames_dropped.load() << "\n\n";

        oss << "# HELP ws_frames_duplicated Total WebSocket frames duplicated\n";
        oss << "# TYPE ws_frames_duplicated counter\n";
        oss << "ws_frames_duplicated " << ws_frames_duplicated.load() << "\n\n";

        return oss.str();
    }
};

inline Metrics& get_metrics() {
    static Metrics metrics;
    return metrics;
}

}
