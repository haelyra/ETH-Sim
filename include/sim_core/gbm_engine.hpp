#pragma once

#include "price_engine.hpp"
#include <random>
#include <cmath>

namespace sim_core {

class GbmPriceEngine : public PriceEngine {
private:
    std::string pair_;
    double price_;
    double drift_;
    double volatility_;
    uint64_t tick_interval_ms_;
    std::mt19937_64 rng_;
    std::normal_distribution<double> normal_;

public:
    GbmPriceEngine(
        std::string pair,
        double initial_price,
        double drift,
        double volatility,
        uint64_t tick_interval_ms,
        std::mt19937_64 rng
    ) : pair_(std::move(pair)),
        price_(initial_price),
        drift_(drift),
        volatility_(volatility),
        tick_interval_ms_(tick_interval_ms),
        rng_(std::move(rng)),
        normal_(0.0, 1.0)
    {}

    PriceMsg next_tick(
        uint64_t ts,
        uint64_t seq,
        SourceKind source,
        uint32_t delay_ms,
        bool stale
    ) override {
        double dt = static_cast<double>(tick_interval_ms_) / 1000.0 / 86400.0 / 365.25;

        double dw = normal_(rng_) * std::sqrt(dt);

        double drift_component = drift_ * dt;
        double diffusion_component = volatility_ * dw;
        double relative_change = drift_component + diffusion_component;

        price_ *= std::exp(relative_change);
        price_ = std::max(price_, 0.01);

        return PriceMsg{
            ts,
            pair_,
            price_,
            source,
            seq,
            delay_ms,
            stale
        };
    }

    double current_price() const override {
        return price_;
    }

    std::string pair() const override {
        return pair_;
    }
};

}
