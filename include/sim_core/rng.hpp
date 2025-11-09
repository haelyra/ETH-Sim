#pragma once

#include <random>
#include <string>
#include <cstdint>
#include <functional>

namespace sim_core {

inline std::mt19937_64 create_labeled_rng(uint64_t seed, const std::string& label) {
    std::hash<std::string> hasher;
    uint64_t label_hash = hasher(label);

    uint64_t final_seed = seed ^ label_hash;

    return std::mt19937_64(final_seed);
}

inline bool happens(std::mt19937_64& rng, double probability) {
    if (probability <= 0.0) return false;
    if (probability >= 1.0) return true;

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < probability;
}

inline uint64_t sample_range(std::mt19937_64& rng, uint64_t min, uint64_t max) {
    if (min >= max) return min;

    std::uniform_int_distribution<uint64_t> dist(min, max);
    return dist(rng);
}

inline double sample_range_f64(std::mt19937_64& rng, double min, double max) {
    if (min >= max) return min;

    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng);
}

}
