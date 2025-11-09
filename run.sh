#!/bin/bash

set -e

if [ ! -f "build/src/dex_sim/dex-sim" ] || [ ! -f "build/src/oracle_sim/oracle-sim" ]; then
    echo "Binaries not found. Please run ./build.sh first"
    exit 1
fi

echo "Cleaning up any existing instances..."
pkill -f dex-sim 2>/dev/null || true
pkill -f oracle-sim 2>/dev/null || true
sleep 1

echo "Starting DEX simulator (port 9101)..."
./build/src/dex_sim/dex-sim > /tmp/dex-sim.log 2>&1 &
DEX_PID=$!

echo "Starting Oracle simulator (port 9102)..."
./build/src/oracle_sim/oracle-sim > /tmp/oracle-sim.log 2>&1 &
ORACLE_PID=$!

sleep 2

if ! kill -0 $DEX_PID 2>/dev/null; then
    echo "DEX simulator failed to start. Check /tmp/dex-sim.log"
    exit 1
fi

if ! kill -0 $ORACLE_PID 2>/dev/null; then
    echo "Oracle simulator failed to start. Check /tmp/oracle-sim.log"
    kill $DEX_PID 2>/dev/null || true
    exit 1
fi

echo ""
echo "Both simulators running."
echo ""
echo "  DEX PID:    $DEX_PID"
echo "  Oracle PID: $ORACLE_PID"
echo ""
echo "  DEX logs:   tail -f /tmp/dex-sim.log"
echo "  Oracle logs: tail -f /tmp/oracle-sim.log"
echo ""
echo "  Visualizer: http://localhost:9101/dual.html"
echo ""
echo "To stop: pkill -f dex-sim; pkill -f oracle-sim"
echo ""

if command -v open &> /dev/null; then
    sleep 1
    open http://localhost:9101/dual.html
fi
