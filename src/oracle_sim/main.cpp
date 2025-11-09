#include <sim_core/types.hpp>
#include <sim_core/config.hpp>
#include <sim_core/rng.hpp>
#include <sim_core/gbm_engine.hpp>
#include <sim_core/metrics.hpp>
#include <sim_core/utils.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <mutex>
#include <vector>
#include <set>
#include <optional>
#include <chrono>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class OracleState {
private:
    sim_core::OracleConfig config_;
    std::unique_ptr<sim_core::PriceEngine> price_engine_;
    mutable std::mutex price_engine_mutex_;

    std::optional<sim_core::PriceMsg> last_price_;
    mutable std::mutex last_price_mutex_;

    std::optional<double> last_published_price_;
    mutable std::mutex last_published_price_mutex_;

    std::optional<std::chrono::steady_clock::time_point> last_publish_time_;
    mutable std::mutex last_publish_time_mutex_;

    std::set<std::shared_ptr<websocket::stream<beast::tcp_stream>>> clients_;
    std::mutex clients_mutex_;

public:
    explicit OracleState(sim_core::OracleConfig config, std::unique_ptr<sim_core::PriceEngine> engine)
        : config_(std::move(config))
        , price_engine_(std::move(engine))
    {}

    const sim_core::OracleConfig& config() const { return config_; }

    void broadcast_price(const sim_core::PriceMsg& msg) {
        {
            std::lock_guard<std::mutex> lock(last_price_mutex_);
            last_price_ = msg;
        }

        spdlog::info("price_tick source={} pair={} price={:.4f} seq={} delay_ms={} stale={}",
            msg.source == sim_core::SourceKind::Chainlink ? "chainlink" : "dex",
            msg.pair, msg.price, msg.src_seq, msg.delay_ms, msg.stale);

        auto ws_msg = sim_core::WsMessage::create_price(msg);
        std::string json_str = ws_msg.to_json_string();

        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            try {
                client->write(asio::buffer(json_str));
            } catch (const std::exception& e) {
                spdlog::warn("Failed to broadcast to client: {}", e.what());
            }
        }
    }

    void add_client(std::shared_ptr<websocket::stream<beast::tcp_stream>> client) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.insert(client);
    }

    void remove_client(std::shared_ptr<websocket::stream<beast::tcp_stream>> client) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(client);
    }

    sim_core::PriceMsg generate_tick(uint64_t ts, uint64_t seq, uint32_t delay_ms, bool stale) {
        std::lock_guard<std::mutex> lock(price_engine_mutex_);
        return price_engine_->next_tick(ts, seq, sim_core::SourceKind::Chainlink, delay_ms, stale);
    }

    std::optional<sim_core::PriceMsg> get_last_price() const {
        std::lock_guard<std::mutex> lock(last_price_mutex_);
        return last_price_;
    }

    bool should_publish(double current_price, std::chrono::steady_clock::time_point now) {
        std::lock_guard<std::mutex> price_lock(last_published_price_mutex_);
        std::lock_guard<std::mutex> time_lock(last_publish_time_mutex_);

        if (!last_published_price_.has_value()) {
            return true;
        }

        double last_price = last_published_price_.value();

        double deviation = std::abs((current_price - last_price) / last_price);
        uint32_t deviation_bps = static_cast<uint32_t>(deviation * 10000.0);

        if (deviation_bps >= config_.oracle_deviation_bps) {
            spdlog::info("Deviation trigger: {} bps (threshold: {})",
                deviation_bps, config_.oracle_deviation_bps);
            return true;
        }

        if (last_publish_time_.has_value()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_publish_time_.value()
            ).count();

            if (static_cast<uint64_t>(elapsed) >= config_.oracle_heartbeat_ms) {
                spdlog::info("Heartbeat trigger: {} ms (threshold: {})",
                    elapsed, config_.oracle_heartbeat_ms);
                return true;
            }
        }

        return false;
    }

    void mark_published(double price, std::chrono::steady_clock::time_point now) {
        {
            std::lock_guard<std::mutex> lock(last_published_price_mutex_);
            last_published_price_ = price;
        }
        {
            std::lock_guard<std::mutex> lock(last_publish_time_mutex_);
            last_publish_time_ = now;
        }
    }

    std::optional<double> get_last_published_price() const {
        std::lock_guard<std::mutex> lock(last_published_price_mutex_);
        return last_published_price_;
    }
};

asio::awaitable<void> run_price_ticker(std::shared_ptr<OracleState> state) {
    auto executor = co_await asio::this_coro::executor;
    const auto& config = state->config();

    auto rng = sim_core::create_labeled_rng(config.server.seed, "ORACLE_TICKER");
    uint64_t seq = 0;
    auto last_tick_time = std::chrono::steady_clock::now();

    while (true) {
        uint64_t tick_ms = sim_core::sample_range(rng, config.oracle_tick_ms.min, config.oracle_tick_ms.max);

        asio::steady_timer timer(executor, std::chrono::milliseconds(tick_ms));
        co_await timer.async_wait(asio::use_awaitable);

        auto now = std::chrono::steady_clock::now();
        uint64_t ts = sim_core::current_time_ms();

        uint32_t delay_ms = static_cast<uint32_t>(
            sim_core::sample_range(rng, config.oracle_ws_jitter_ms.min, config.oracle_ws_jitter_ms.max)
        );

        auto elapsed_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_tick_time
        ).count();
        bool stale = static_cast<uint64_t>(elapsed_since_last) > config.oracle_stale_after_ms;

        auto msg = state->generate_tick(ts, seq, delay_ms, stale);
        double current_price = msg.price;

        bool should_pub = state->should_publish(current_price, now);

        if (!should_pub) {
            auto last_pub = state->get_last_published_price();
            if (last_pub.has_value()) {
                double dev_bps = std::abs((current_price - last_pub.value()) / last_pub.value()) * 10000.0;
                spdlog::debug(
                    "Price check: current={:.4f}, last_published={:.4f}, deviation={:.2f} bps (threshold={})",
                    current_price, last_pub.value(), dev_bps, config.oracle_deviation_bps
                );
            }
        }

        if (!should_pub) {
            last_tick_time = now;
            continue;
        }

        sim_core::get_metrics().price_ticks_generated++;

        if (sim_core::happens(rng, config.oracle_p_drop)) {
            sim_core::get_metrics().ws_frames_dropped++;
            state->mark_published(msg.price, now);
            seq++;
            last_tick_time = now;
            continue;
        }

        state->broadcast_price(msg);
        state->mark_published(msg.price, now);
        sim_core::get_metrics().ws_frames_sent++;
        seq++;

        if (sim_core::happens(rng, config.oracle_p_dup)) {
            state->broadcast_price(msg);
            sim_core::get_metrics().ws_frames_duplicated++;
        }

        last_tick_time = now;
    }
}

asio::awaitable<void> handle_websocket_session(
    tcp::socket socket,
    std::shared_ptr<OracleState> state,
    std::optional<http::request<http::string_body>> initial_req = std::nullopt)
{
    try {
        auto ws = std::make_shared<websocket::stream<beast::tcp_stream>>(std::move(socket));

        if (initial_req.has_value()) {
            co_await ws->async_accept(*initial_req, asio::use_awaitable);
        } else {
            co_await ws->async_accept(asio::use_awaitable);
        }

        state->add_client(ws);

        auto sub_msg = sim_core::WsMessage::create_subscription("oracle_prices", "subscribed");
        std::string sub_json = sub_msg.to_json_string();
        co_await ws->async_write(asio::buffer(sub_json), asio::use_awaitable);

        beast::flat_buffer buffer;
        while (true) {
            co_await ws->async_read(buffer, asio::use_awaitable);
            buffer.clear();
        }
    } catch (const std::exception& e) {
    }
}

http::message_generator handle_http_request(
    http::request<http::string_body> req,
    std::shared_ptr<OracleState> state)
{
    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, "oracle-sim");
        res.set(http::field::content_type, "text/plain");
        res.keep_alive(req.keep_alive());
        res.body() = "Not found: " + std::string(target);
        res.prepare_payload();
        return res;
    };

    auto const ok_json = [&req](const std::string& json) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "oracle-sim");
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = json;
        res.prepare_payload();
        return res;
    };

    auto const ok_text = [&req](const std::string& text) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "oracle-sim");
        res.set(http::field::content_type, "text/plain");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = text;
        res.prepare_payload();
        return res;
    };

    std::string target(req.target());

    if (target == "/healthz") {
        return ok_text("OK");
    }

    if (target == "/metrics") {
        return ok_text(sim_core::get_metrics().to_prometheus());
    }

    if (target == "/oracle/snapshot") {
        sim_core::PriceSnapshot snapshot;
        if (auto price = state->get_last_price()) {
            snapshot.prices.push_back(*price);
        }
        snapshot.server_time = sim_core::current_time_ms();

        nlohmann::json j = snapshot;
        return ok_json(j.dump());
    }

    return not_found(req.target());
}

asio::awaitable<void> handle_http_session(
    tcp::socket socket,
    std::shared_ptr<OracleState> state)
{
    try {
        beast::tcp_stream stream(std::move(socket));
        beast::flat_buffer buffer;

        while (true) {
            stream.expires_after(std::chrono::seconds(30));

            http::request<http::string_body> req;
            co_await http::async_read(stream, buffer, req, asio::use_awaitable);

            if (websocket::is_upgrade(req)) {
                auto raw_socket = stream.release_socket();
                co_await handle_websocket_session(
                    std::move(raw_socket),
                    state,
                    std::move(req)
                );
                co_return;
            }

            auto response = handle_http_request(std::move(req), state);
            co_await beast::async_write(stream, std::move(response), asio::use_awaitable);

            if (!req.keep_alive()) {
                break;
            }
        }
    } catch (const std::exception&) {
    }
}

asio::awaitable<void> listen(
    tcp::acceptor& acceptor,
    std::shared_ptr<OracleState> state)
{
    while (true) {
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);

        asio::co_spawn(
            acceptor.get_executor(),
            handle_http_session(std::move(socket), state),
            asio::detached
        );
    }
}

int main(int argc, char* argv[]) {
    try {
        auto console = spdlog::stdout_color_mt("console");
        spdlog::set_default_logger(console);
        spdlog::set_level(spdlog::level::info);

        std::string config_path = argc > 1 ? argv[1] : "configs/oracle.yaml";
        auto config = sim_core::load_oracle_config(config_path);

        spdlog::info("🟠 Oracle Simulator Starting");
        spdlog::info("  WS:     ws://{}/ws/prices", config.server.http_bind);
        spdlog::info("  HTTP:   http://{}/oracle/snapshot", config.server.http_bind);
        spdlog::info("  Metrics: http://{}/metrics", config.server.http_bind);
        spdlog::info("  Model:  {}", config.server.price_model);
        spdlog::info("  Seed:   {}", config.server.seed);
        spdlog::info("  Deviation threshold: {} bps", config.oracle_deviation_bps);
        spdlog::info("  Heartbeat: {} ms", config.oracle_heartbeat_ms);

        auto rng = sim_core::create_labeled_rng(config.server.seed, "ORACLE");
        auto engine = std::make_unique<sim_core::GbmPriceEngine>(
            config.server.pairs[0],
            config.server.price_start,
            config.server.gbm_mu,
            config.server.gbm_sigma,
            config.oracle_tick_ms.min,
            std::move(rng)
        );

        auto state = std::make_shared<OracleState>(std::move(config), std::move(engine));

        auto [host, port] = sim_core::parse_bind_address(state->config().server.http_bind);

        asio::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(asio::ip::make_address(host), port));

        asio::co_spawn(ioc, run_price_ticker(state), asio::detached);

        asio::co_spawn(ioc, listen(acceptor, state), asio::detached);

        spdlog::info("🚀 Oracle server ready");

        ioc.run();

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
