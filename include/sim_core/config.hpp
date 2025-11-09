#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <yaml-cpp/yaml.h>

namespace sim_core {

template<typename T>
struct Range {
    T min;
    T max;
};

struct ServerConfig {
    std::vector<std::string> pairs;
    std::string price_model;
    double price_start;
    double gbm_mu;
    double gbm_sigma;
    double jump_lambda;
    double jump_mu;
    double jump_sigma;
    uint64_t seed;
    std::string ws_bind;
    std::string http_bind;
    std::vector<std::string> cors_allow_origins;
};

struct DexConfig {
    ServerConfig server;
    Range<uint64_t> dex_tick_ms;
    Range<uint64_t> dex_ws_jitter_ms;
    Range<uint64_t> dex_latency_ms;
    double dex_p_drop;
    double dex_p_dup;
    double dex_p_reorder;
    bool dex_burst_mode;
    uint64_t dex_burst_on_ms;
    uint64_t dex_burst_off_ms;
    std::vector<uint64_t> dex_disconnect_windows_ms;
    uint64_t dex_stale_after_ms;
};

struct OracleConfig {
    ServerConfig server;
    Range<uint64_t> oracle_tick_ms;
    uint32_t oracle_deviation_bps;
    uint64_t oracle_heartbeat_ms;
    Range<uint64_t> oracle_ws_jitter_ms;
    double oracle_p_drop;
    double oracle_p_dup;
    double oracle_p_reorder;
    uint64_t oracle_stale_after_ms;
};

template<typename T>
Range<T> load_range(const YAML::Node& node) {
    return Range<T>{
        node["min"].as<T>(),
        node["max"].as<T>()
    };
}

inline ServerConfig load_server_config(const YAML::Node& config) {
    ServerConfig sc;

    sc.pairs = config["pairs"].as<std::vector<std::string>>();
    sc.price_model = config["price_model"].as<std::string>();
    sc.price_start = config["price_start"].as<double>();
    sc.gbm_mu = config["gbm_mu"].as<double>();
    sc.gbm_sigma = config["gbm_sigma"].as<double>();
    sc.jump_lambda = config["jump_lambda"].as<double>();
    sc.jump_mu = config["jump_mu"].as<double>();
    sc.jump_sigma = config["jump_sigma"].as<double>();
    sc.seed = config["seed"].as<uint64_t>();
    sc.ws_bind = config["ws_bind"].as<std::string>();
    sc.http_bind = config["http_bind"].as<std::string>();
    sc.cors_allow_origins = config["cors_allow_origins"].as<std::vector<std::string>>();

    return sc;
}

inline DexConfig load_dex_config(const std::string& config_path = "configs/dex.yaml") {
    YAML::Node config = YAML::LoadFile(config_path);

    DexConfig dc;
    dc.server = load_server_config(config);
    dc.dex_tick_ms = load_range<uint64_t>(config["dex_tick_ms"]);
    dc.dex_ws_jitter_ms = load_range<uint64_t>(config["dex_ws_jitter_ms"]);
    dc.dex_latency_ms = load_range<uint64_t>(config["dex_latency_ms"]);
    dc.dex_p_drop = config["dex_p_drop"].as<double>();
    dc.dex_p_dup = config["dex_p_dup"].as<double>();
    dc.dex_p_reorder = config["dex_p_reorder"].as<double>();
    dc.dex_burst_mode = config["dex_burst_mode"].as<bool>();
    dc.dex_burst_on_ms = config["dex_burst_on_ms"].as<uint64_t>();
    dc.dex_burst_off_ms = config["dex_burst_off_ms"].as<uint64_t>();
    dc.dex_disconnect_windows_ms = config["dex_disconnect_windows_ms"].as<std::vector<uint64_t>>();
    dc.dex_stale_after_ms = config["dex_stale_after_ms"].as<uint64_t>();

    return dc;
}

inline OracleConfig load_oracle_config(const std::string& config_path = "configs/oracle.yaml") {
    YAML::Node config = YAML::LoadFile(config_path);

    OracleConfig oc;
    oc.server = load_server_config(config);
    oc.oracle_tick_ms = load_range<uint64_t>(config["oracle_tick_ms"]);
    oc.oracle_deviation_bps = config["oracle_deviation_bps"].as<uint32_t>();
    oc.oracle_heartbeat_ms = config["oracle_heartbeat_ms"].as<uint64_t>();
    oc.oracle_ws_jitter_ms = load_range<uint64_t>(config["oracle_ws_jitter_ms"]);
    oc.oracle_p_drop = config["oracle_p_drop"].as<double>();
    oc.oracle_p_dup = config["oracle_p_dup"].as<double>();
    oc.oracle_p_reorder = config["oracle_p_reorder"].as<double>();
    oc.oracle_stale_after_ms = config["oracle_stale_after_ms"].as<uint64_t>();

    return oc;
}

}
