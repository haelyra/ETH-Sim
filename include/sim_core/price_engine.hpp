#pragma once

#include "types.hpp"
#include <memory>

namespace sim_core {

class PriceEngine {
public:
    virtual ~PriceEngine() = default;

    virtual PriceMsg next_tick(
        uint64_t ts,
        uint64_t seq,
        SourceKind source,
        uint32_t delay_ms,
        bool stale
    ) = 0;

    virtual double current_price() const = 0;

    virtual std::string pair() const = 0;
};

using PriceEnginePtr = std::unique_ptr<PriceEngine>;

}
