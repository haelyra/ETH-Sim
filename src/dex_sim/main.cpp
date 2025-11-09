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
#include <fstream>
#include <chrono>
#include <optional>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class DexState {
private:
    sim_core::DexConfig config_;
    std::unique_ptr<sim_core::PriceEngine> price_engine_;
    mutable std::mutex price_engine_mutex_;

    std::optional<sim_core::PriceMsg> last_price_;
    mutable std::mutex last_price_mutex_;

    std::set<std::shared_ptr<websocket::stream<beast::tcp_stream>>> clients_;
    std::mutex clients_mutex_;

public:
    explicit DexState(sim_core::DexConfig config, std::unique_ptr<sim_core::PriceEngine> engine)
        : config_(std::move(config))
        , price_engine_(std::move(engine))
    {}

    const sim_core::DexConfig& config() const { return config_; }

    void broadcast_price(const sim_core::PriceMsg& msg) {
        {
            std::lock_guard<std::mutex> lock(last_price_mutex_);
            last_price_ = msg;
        }

        spdlog::info("price_tick source={} pair={} price={:.4f} seq={} delay_ms={} stale={}",
            msg.source == sim_core::SourceKind::Dex ? "dex" : "chainlink",
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
        return price_engine_->next_tick(ts, seq, sim_core::SourceKind::Dex, delay_ms, stale);
    }

    std::optional<sim_core::PriceMsg> get_last_price() const {
        std::lock_guard<std::mutex> lock(last_price_mutex_);
        return last_price_;
    }
};

asio::awaitable<void> run_price_ticker(std::shared_ptr<DexState> state) {
    auto executor = co_await asio::this_coro::executor;
    const auto& config = state->config();

    auto rng = sim_core::create_labeled_rng(config.server.seed, "DEX_TICKER");
    uint64_t seq = 0;
    auto last_tick_time = std::chrono::steady_clock::now();

    while (true) {
        uint64_t tick_ms = sim_core::sample_range(rng, config.dex_tick_ms.min, config.dex_tick_ms.max);

        if (config.dex_burst_mode) {
            bool burst_on = sim_core::happens(rng, 0.5);
            if (burst_on) {
                tick_ms = std::min(tick_ms, config.dex_burst_on_ms);
            } else {
                tick_ms = std::max(tick_ms, config.dex_burst_off_ms);
            }
        }

        asio::steady_timer timer(executor, std::chrono::milliseconds(tick_ms));
        co_await timer.async_wait(asio::use_awaitable);

        auto now = std::chrono::steady_clock::now();
        uint64_t ts = sim_core::current_time_ms();

        uint32_t delay_ms = static_cast<uint32_t>(
            sim_core::sample_range(rng, config.dex_latency_ms.min, config.dex_latency_ms.max)
        );

        auto elapsed_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_tick_time
        ).count();
        bool stale = static_cast<uint64_t>(elapsed_since_last) > config.dex_stale_after_ms;

        auto msg = state->generate_tick(ts, seq, delay_ms, stale);

        sim_core::get_metrics().price_ticks_generated++;

        if (sim_core::happens(rng, config.dex_p_drop)) {
            sim_core::get_metrics().ws_frames_dropped++;
            seq++;
            last_tick_time = now;
            continue;
        }

        state->broadcast_price(msg);
        sim_core::get_metrics().ws_frames_sent++;
        seq++;

        if (sim_core::happens(rng, config.dex_p_dup)) {
            state->broadcast_price(msg);
            sim_core::get_metrics().ws_frames_duplicated++;
        }

        last_tick_time = now;
    }
}

asio::awaitable<void> handle_websocket_session(
    tcp::socket socket,
    std::shared_ptr<DexState> state,
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

        auto sub_msg = sim_core::WsMessage::create_subscription("dex_ticks", "subscribed");
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
    std::shared_ptr<DexState> state)
{
    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, "dex-sim");
        res.set(http::field::content_type, "text/plain");
        res.keep_alive(req.keep_alive());
        res.body() = "Not found: " + std::string(target);
        res.prepare_payload();
        return res;
    };

    auto const ok_json = [&req](const std::string& json) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "dex-sim");
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = json;
        res.prepare_payload();
        return res;
    };

    auto const ok_text = [&req](const std::string& text) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "dex-sim");
        res.set(http::field::content_type, "text/plain");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = text;
        res.prepare_payload();
        return res;
    };

    auto const ok_html = [&req](const std::string& html) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "dex-sim");
        res.set(http::field::content_type, "text/html");
        res.set(http::field::access_control_allow_origin, "*");
        res.keep_alive(req.keep_alive());
        res.body() = html;
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

    if (target == "/prices/snapshot") {
        sim_core::PriceSnapshot snapshot;
        if (auto price = state->get_last_price()) {
            snapshot.prices.push_back(*price);
        }
        snapshot.server_time = sim_core::current_time_ms();

        nlohmann::json j = snapshot;
        return ok_json(j.dump());
    }

    auto load_static_file = [](const std::string& filename) -> std::optional<std::string> {
        std::ifstream file("static/" + filename);
        if (!file) return std::nullopt;
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };

    if (target == "/" || target == "/index.html") {
        if (auto content = load_static_file("index.html")) {
            return ok_html(*content);
        }
    }
    if (target == "/dual.html") {
        if (auto content = load_static_file("dual.html")) {
            return ok_html(*content);
        }
    }
    if (target == "/debug.html") {
        if (auto content = load_static_file("debug.html")) {
            return ok_html(*content);
        }
    }

    return not_found(req.target());
}

asio::awaitable<void> handle_http_session(
    tcp::socket socket,
    std::shared_ptr<DexState> state)
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
    std::shared_ptr<DexState> state)
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

        std::string config_path = argc > 1 ? argv[1] : "configs/dex.yaml";
        auto config = sim_core::load_dex_config(config_path);

        spdlog::info("🔵 DEX Simulator Starting");
        spdlog::info("  WS:     ws://{}/ws/ticks", config.server.http_bind);
        spdlog::info("  HTTP:   http://{}/prices/snapshot", config.server.http_bind);
        spdlog::info("  Metrics: http://{}/metrics", config.server.http_bind);
        spdlog::info("  Model:  {}", config.server.price_model);
        spdlog::info("  Seed:   {}", config.server.seed);

        auto rng = sim_core::create_labeled_rng(config.server.seed, "DEX");
        auto engine = std::make_unique<sim_core::GbmPriceEngine>(
            config.server.pairs[0],
            config.server.price_start,
            config.server.gbm_mu,
            config.server.gbm_sigma,
            config.dex_tick_ms.min,
            std::move(rng)
        );

        auto state = std::make_shared<DexState>(std::move(config), std::move(engine));

        auto [host, port] = sim_core::parse_bind_address(state->config().server.http_bind);

        asio::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(asio::ip::make_address(host), port));

        asio::co_spawn(ioc, run_price_ticker(state), asio::detached);

        asio::co_spawn(ioc, listen(acceptor, state), asio::detached);

        spdlog::info("🚀 DEX server ready");

        ioc.run();

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
