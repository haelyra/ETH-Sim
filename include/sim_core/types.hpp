#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

namespace sim_core {

enum class SourceKind {
    Dex,
    Chainlink
};

struct PriceMsg {
    uint64_t ts;
    std::string pair;
    double price;
    SourceKind source;
    uint64_t src_seq;
    uint32_t delay_ms;
    bool stale;
};

struct SubscriptionMsg {
    std::string id;
    std::string status;
};

struct PriceSnapshot {
    std::vector<PriceMsg> prices;
    uint64_t server_time;
};

inline void to_json(nlohmann::json& j, const PriceMsg& p) {
    j = nlohmann::json{
        {"ts", p.ts},
        {"pair", p.pair},
        {"price", p.price},
        {"source", p.source == SourceKind::Dex ? "dex" : "chainlink"},
        {"src_seq", p.src_seq},
        {"delay_ms", p.delay_ms},
        {"stale", p.stale}
    };
}

inline void from_json(const nlohmann::json& j, PriceMsg& p) {
    j.at("ts").get_to(p.ts);
    j.at("pair").get_to(p.pair);
    j.at("price").get_to(p.price);

    std::string source_str = j.at("source").get<std::string>();
    p.source = (source_str == "dex") ? SourceKind::Dex : SourceKind::Chainlink;

    j.at("src_seq").get_to(p.src_seq);
    j.at("delay_ms").get_to(p.delay_ms);
    j.at("stale").get_to(p.stale);
}

inline void to_json(nlohmann::json& j, const SubscriptionMsg& s) {
    j = nlohmann::json{
        {"type", "subscription"},
        {"id", s.id},
        {"status", s.status}
    };
}

inline void to_json(nlohmann::json& j, const PriceSnapshot& s) {
    j = nlohmann::json{
        {"prices", s.prices},
        {"server_time", s.server_time}
    };
}

struct WsMessage {
    enum class Type {
        Price,
        Subscription
    };

    Type type;
    PriceMsg price;
    SubscriptionMsg subscription;

    static WsMessage create_price(const PriceMsg& p) {
        WsMessage msg;
        msg.type = Type::Price;
        msg.price = p;
        return msg;
    }

    static WsMessage create_subscription(const std::string& id, const std::string& status) {
        WsMessage msg;
        msg.type = Type::Subscription;
        msg.subscription = {id, status};
        return msg;
    }

    std::string to_json_string() const {
        nlohmann::json j;
        if (type == Type::Price) {
            j = price;
            j["type"] = "price";
        } else {
            j = subscription;
        }
        return j.dump();
    }
};

}
