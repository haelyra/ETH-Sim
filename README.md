# ETH-sim C++ - Ethereum Price Simulator

Fast dual-server price feed simulator for testing MEV bots and liquidation strategies.

## Overview

**ETH-sim** simulates two different price feeds for Ethereum:
- **DEX Feed**: High frequency noisy market data (10-100ms updates)
- **Oracle Feed**: Chainlink deviation/heartbeat updates (triggers on ≥0.05% price change or 1 hr heartbeat)

Made for testing arbitrage bots, liquidation systems, and oracle-dependent DeFi strategies in a controlled environment.

Tune feed parameters in /configs as desired.

## Quick Start

### Prereqs

- macOS
- homebrew

### Installation

```bash
# 1. Install dependencies (one-time setup)
./INSTALL_DEPS.sh

# 2. Build the project
./build.sh
```

**manual build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# DEX 
./build/src/dex_sim/dex-sim

# oracle
./build/src/oracle_sim/oracle-sim

# visualizer
open http://localhost:9101/dual.html
```

## AI summary:
# Features

- ✅ **Dual WebSocket Feeds**: DEX (10-100ms) + Oracle (deviation/heartbeat)
- ✅ **Geometric Brownian Motion**: Realistic price simulation (200% volatility)
- ✅ **Deviation Triggers**: Chainlink-like updates on ≥N bps price change
- ✅ **Heartbeat Triggers**: Guaranteed updates every N seconds
- ✅ **Real-time Visualizer**: Beautiful dual-feed chart in browser
- ✅ **Deterministic RNG**: Reproducible simulations
- ✅ **Fault Injection**: Packet loss/duplication for stress testing
- ✅ **Prometheus Metrics**: Production-ready monitoring
- ✅ **100% Feature Parity**: Identical to Rust version

## Architecture

```
DEX Simulator (9101)          Oracle Simulator (9102)
├── GBM Price Engine          ├── GBM Price Engine
├── High Frequency (10-100ms) ├── Deviation Check (±N bps)
├── Burst Mode Support        ├── Heartbeat (N seconds)
├── Fault Injection           ├── Fault Injection
└── WebSocket /ws/ticks       └── WebSocket /ws/prices
         │                             │
         └─────────┬───────────────────┘
                   │
            Browser Visualizer
            (dual.html)
```

## API Endpoints

### DEX (Port 9101)
- `GET /healthz` - Health check
- `GET /metrics` - Prometheus metrics
- `GET /prices/snapshot` - Latest price (JSON)
- `WebSocket /ws/ticks` - Real-time stream
- `GET /dual.html` - Visualizer

### Oracle (Port 9102)
- `GET /healthz` - Health check
- `GET /metrics` - Prometheus metrics
- `GET /oracle/snapshot` - Latest oracle price (JSON)
- `WebSocket /ws/prices` - Real-time stream

## Configuration

### DEX (`configs/dex.yaml`)

```yaml
price_start: 3500.0      # Must match Oracle
gbm_sigma: 2.0           # 200% volatility (demo)

dex_tick_ms:
  min: 10                # High frequency
  max: 100

dex_burst_mode: true     # Alternates fast/slow
dex_p_drop: 0.02         # 2% packet loss
```

### Oracle (`configs/oracle.yaml`)

```yaml
price_start: 3500.0           # Must match DEX
gbm_sigma: 2.0                # Same as DEX

oracle_tick_ms:
  min: 1000                   # Check every 1-3.6s
  max: 3600

oracle_deviation_bps: 1       # 0.01% trigger (demo)
                              # Use 10-50 for realistic
oracle_heartbeat_ms: 3600000  # 1 hour
```

## Testing

```bash
# Run all tests (unit + integration)
./run_all_tests.sh

# Unit tests only (19 tests)
./build/tests/test_core

# Integration tests only
./tests/integration_test.sh
```

## WebSocket Message Format

```json
{
  "type": "price",
  "ts": 1699123456789,
  "pair": "ETH/USD",
  "price": 3500.50,
  "source": "dex",
  "src_seq": 42,
  "delay_ms": 10,
  "stale": false
}
```

## Use Cases

**MEV Bot Testing**: Test frontrunning against realistic DEX/Oracle spreads
**Liquidation Bots**: Validate triggers with Oracle price updates
**Oracle Integration**: Test Chainlink-dependent logic offline
**Stress Testing**: Fault injection for network reliability testing

## Troubleshooting

**CMake cache errors**: Delete `build/` directory and run `./build.sh` again
```bash
rm -rf build && ./build.sh
```

**Build fails**: Run `./INSTALL_DEPS.sh` to install dependencies
```bash
./INSTALL_DEPS.sh
```

**Port already in use**: Kill existing instances
```bash
pkill -f dex-sim; pkill -f oracle-sim
```

**Visualizer disconnected**: Check servers are running
```bash
curl http://localhost:9101/healthz
curl http://localhost:9102/healthz
```

**No Oracle updates**: Lower `oracle_deviation_bps` to 1-10 bps in `configs/oracle.yaml`

**Check logs**: If using `./run.sh`, logs are in `/tmp/dex-sim.log` and `/tmp/oracle-sim.log`