// Unit tests for sim_core library
// Tests: types, config, RNG, price engines, metrics

#include <sim_core/types.hpp>
#include <sim_core/config.hpp>
#include <sim_core/rng.hpp>
#include <sim_core/gbm_engine.hpp>
#include <sim_core/metrics.hpp>
#include <sim_core/utils.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// Test: PriceMsg JSON serialization
TEST(TypesTest, PriceMsgSerialization) {
    sim_core::PriceMsg msg{
        .ts = 1234567890,
        .pair = "ETH/USD",
        .price = 3500.50,
        .source = sim_core::SourceKind::Dex,
        .src_seq = 42,
        .delay_ms = 10,
        .stale = false
    };

    nlohmann::json j = msg;

    EXPECT_EQ(j["ts"], 1234567890);
    EXPECT_EQ(j["pair"], "ETH/USD");
    EXPECT_DOUBLE_EQ(j["price"], 3500.50);
    EXPECT_EQ(j["source"], "dex");
    EXPECT_EQ(j["src_seq"], 42);
    EXPECT_EQ(j["delay_ms"], 10);
    EXPECT_EQ(j["stale"], false);
}

// Test: WsMessage creation
TEST(TypesTest, WsMessageCreation) {
    sim_core::PriceMsg price_msg{
        .ts = 1000,
        .pair = "ETH/USD",
        .price = 3500.0,
        .source = sim_core::SourceKind::Chainlink,
        .src_seq = 1,
        .delay_ms = 5,
        .stale = false
    };

    auto ws_msg = sim_core::WsMessage::create_price(price_msg);
    std::string json_str = ws_msg.to_json_string();

    EXPECT_TRUE(json_str.find("\"type\":\"price\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"source\":\"chainlink\"") != std::string::npos);
}

TEST(TypesTest, WsMessageSubscription) {
    auto ws_msg = sim_core::WsMessage::create_subscription("test_feed", "subscribed");
    std::string json_str = ws_msg.to_json_string();

    EXPECT_TRUE(json_str.find("\"type\":\"subscription\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"id\":\"test_feed\"") != std::string::npos);
    EXPECT_TRUE(json_str.find("\"status\":\"subscribed\"") != std::string::npos);
}

// Test: Deterministic RNG
TEST(RngTest, Determinism) {
    auto rng1 = sim_core::create_labeled_rng(42, "TEST");
    auto rng2 = sim_core::create_labeled_rng(42, "TEST");

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(sim_core::sample_range(rng1, 0, 1000),
                  sim_core::sample_range(rng2, 0, 1000));
    }
}

TEST(RngTest, DifferentLabels) {
    auto rng1 = sim_core::create_labeled_rng(42, "LABEL_A");
    auto rng2 = sim_core::create_labeled_rng(42, "LABEL_B");

    // Different labels should produce different sequences
    bool all_same = true;
    for (int i = 0; i < 100; ++i) {
        if (sim_core::sample_range(rng1, 0, 1000) !=
            sim_core::sample_range(rng2, 0, 1000)) {
            all_same = false;
            break;
        }
    }
    EXPECT_FALSE(all_same);
}

TEST(RngTest, SampleRange) {
    auto rng = sim_core::create_labeled_rng(42, "TEST");

    for (int i = 0; i < 1000; ++i) {
        uint64_t val = sim_core::sample_range(rng, 10, 100);
        EXPECT_GE(val, 10);
        EXPECT_LE(val, 100);
    }
}

TEST(RngTest, Happens) {
    auto rng = sim_core::create_labeled_rng(42, "TEST");

    // Test probability 0.0 (never happens)
    int count_zero = 0;
    for (int i = 0; i < 100; ++i) {
        if (sim_core::happens(rng, 0.0)) count_zero++;
    }
    EXPECT_EQ(count_zero, 0);

    // Test probability 1.0 (always happens)
    int count_one = 0;
    for (int i = 0; i < 100; ++i) {
        if (sim_core::happens(rng, 1.0)) count_one++;
    }
    EXPECT_EQ(count_one, 100);

    // Test probability 0.5 (approximately 50%)
    auto rng2 = sim_core::create_labeled_rng(123, "TEST");
    int count_half = 0;
    for (int i = 0; i < 10000; ++i) {
        if (sim_core::happens(rng2, 0.5)) count_half++;
    }
    EXPECT_GT(count_half, 4500);
    EXPECT_LT(count_half, 5500);
}

// Test: GBM Price Engine determinism
TEST(GbmEngineTest, Determinism) {
    auto rng1 = sim_core::create_labeled_rng(42, "TEST");
    auto rng2 = sim_core::create_labeled_rng(42, "TEST");

    sim_core::GbmPriceEngine engine1("ETH/USD", 3500.0, 0.0, 2.0, 1000, std::move(rng1));
    sim_core::GbmPriceEngine engine2("ETH/USD", 3500.0, 0.0, 2.0, 1000, std::move(rng2));

    for (int i = 0; i < 10; ++i) {
        auto tick1 = engine1.next_tick(i * 1000, i, sim_core::SourceKind::Dex, 0, false);
        auto tick2 = engine2.next_tick(i * 1000, i, sim_core::SourceKind::Dex, 0, false);

        EXPECT_DOUBLE_EQ(tick1.price, tick2.price);
        EXPECT_EQ(tick1.pair, tick2.pair);
        EXPECT_EQ(tick1.source, tick2.source);
    }
}

TEST(GbmEngineTest, PriceMovement) {
    auto rng = sim_core::create_labeled_rng(42, "TEST");
    sim_core::GbmPriceEngine engine("ETH/USD", 3500.0, 0.0, 2.0, 1000, std::move(rng));

    // Generate 100 ticks and check prices are positive and varying
    std::vector<double> prices;
    for (int i = 0; i < 100; ++i) {
        auto tick = engine.next_tick(i * 1000, i, sim_core::SourceKind::Dex, 0, false);
        prices.push_back(tick.price);
        EXPECT_GT(tick.price, 0.0);  // Price must be positive
    }

    // Prices should vary (not all the same)
    bool has_variation = false;
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i] != prices[0]) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);
}

TEST(GbmEngineTest, CurrentPrice) {
    auto rng = sim_core::create_labeled_rng(42, "TEST");
    sim_core::GbmPriceEngine engine("ETH/USD", 3500.0, 0.0, 2.0, 1000, std::move(rng));

    EXPECT_DOUBLE_EQ(engine.current_price(), 3500.0);

    auto tick = engine.next_tick(1000, 0, sim_core::SourceKind::Dex, 0, false);
    EXPECT_DOUBLE_EQ(engine.current_price(), tick.price);
}

TEST(GbmEngineTest, Pair) {
    auto rng = sim_core::create_labeled_rng(42, "TEST");
    sim_core::GbmPriceEngine engine("BTC/USD", 50000.0, 0.0, 1.5, 1000, std::move(rng));

    EXPECT_EQ(engine.pair(), "BTC/USD");
}

// Test: Metrics
TEST(MetricsTest, Counters) {
    auto& metrics = sim_core::get_metrics();
    metrics.reset();

    EXPECT_EQ(metrics.price_ticks_generated.load(), 0);
    EXPECT_EQ(metrics.ws_frames_sent.load(), 0);

    metrics.price_ticks_generated++;
    metrics.ws_frames_sent += 5;

    EXPECT_EQ(metrics.price_ticks_generated.load(), 1);
    EXPECT_EQ(metrics.ws_frames_sent.load(), 5);

    metrics.reset();
    EXPECT_EQ(metrics.price_ticks_generated.load(), 0);
}

TEST(MetricsTest, PrometheusFormat) {
    auto& metrics = sim_core::get_metrics();
    metrics.reset();

    metrics.price_ticks_generated = 100;
    metrics.ws_frames_sent = 95;
    metrics.ws_frames_dropped = 3;
    metrics.ws_frames_duplicated = 2;

    std::string prom = metrics.to_prometheus();

    EXPECT_TRUE(prom.find("price_ticks_generated 100") != std::string::npos);
    EXPECT_TRUE(prom.find("ws_frames_sent 95") != std::string::npos);
    EXPECT_TRUE(prom.find("ws_frames_dropped 3") != std::string::npos);
    EXPECT_TRUE(prom.find("ws_frames_duplicated 2") != std::string::npos);

    metrics.reset();
}

// Test: Utils
TEST(UtilsTest, CurrentTimeMs) {
    auto t1 = sim_core::current_time_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto t2 = sim_core::current_time_ms();

    EXPECT_GT(t2, t1);
    EXPECT_GE(t2 - t1, 10);  // At least 10ms elapsed
}

TEST(UtilsTest, ParseBindAddress) {
    auto [host1, port1] = sim_core::parse_bind_address("127.0.0.1:9101");
    EXPECT_EQ(host1, "127.0.0.1");
    EXPECT_EQ(port1, 9101);

    auto [host2, port2] = sim_core::parse_bind_address("0.0.0.0:8080");
    EXPECT_EQ(host2, "0.0.0.0");
    EXPECT_EQ(port2, 8080);
}

TEST(UtilsTest, ParseBindAddressInvalid) {
    EXPECT_THROW(sim_core::parse_bind_address("invalid"), std::runtime_error);
    EXPECT_THROW(sim_core::parse_bind_address("127.0.0.1"), std::runtime_error);
}

// Test: Config loading
TEST(ConfigTest, LoadDexConfig) {
    // This test requires configs/dex.yaml to exist
    try {
        auto config = sim_core::load_dex_config("configs/dex.yaml");

        EXPECT_FALSE(config.server.pairs.empty());
        EXPECT_GT(config.server.price_start, 0.0);
        EXPECT_GE(config.dex_tick_ms.min, 0);
        EXPECT_GE(config.dex_tick_ms.max, config.dex_tick_ms.min);
        EXPECT_GE(config.dex_p_drop, 0.0);
        EXPECT_LE(config.dex_p_drop, 1.0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Config file not found: " << e.what();
    }
}

TEST(ConfigTest, LoadOracleConfig) {
    // This test requires configs/oracle.yaml to exist
    try {
        auto config = sim_core::load_oracle_config("configs/oracle.yaml");

        EXPECT_FALSE(config.server.pairs.empty());
        EXPECT_GT(config.server.price_start, 0.0);
        EXPECT_GE(config.oracle_tick_ms.min, 0);
        EXPECT_GE(config.oracle_tick_ms.max, config.oracle_tick_ms.min);
        EXPECT_GT(config.oracle_deviation_bps, 0);
        EXPECT_GT(config.oracle_heartbeat_ms, 0);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Config file not found: " << e.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
