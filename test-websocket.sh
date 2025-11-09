#!/bin/bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0

pass() {
    echo -e "${GREEN}✓${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

if ! command -v websocat &> /dev/null; then
    echo "websocat not found. Install with: brew install websocat"
    exit 1
fi

echo "WebSocket Connectivity Tests"
echo "============================="
echo ""

pkill -f "dex-sim" || true
pkill -f "oracle-sim" || true
sleep 1

echo "Starting DEX simulator..."
./build/src/dex_sim/dex-sim configs/dex.yaml > /tmp/dex-sim.log 2>&1 &
DEX_PID=$!

echo "Starting Oracle simulator..."
./build/src/oracle_sim/oracle-sim configs/oracle.yaml > /tmp/oracle-sim.log 2>&1 &
ORACLE_PID=$!

sleep 3

if ! ps -p $DEX_PID > /dev/null; then
    fail "DEX simulator failed to start"
    cat /tmp/dex-sim.log
    kill $ORACLE_PID 2>/dev/null || true
    exit 1
fi

if ! ps -p $ORACLE_PID > /dev/null; then
    fail "Oracle simulator failed to start"
    cat /tmp/oracle-sim.log
    kill $DEX_PID 2>/dev/null || true
    exit 1
fi

echo "Testing DEX WebSocket..."
if command -v gtimeout &> /dev/null; then
    echo "" | gtimeout 5s websocat ws://127.0.0.1:9101/ws/ticks > /tmp/dex-ws.log 2>&1 &
    WS_DEX_PID=$!
elif command -v timeout &> /dev/null; then
    echo "" | timeout 5s websocat ws://127.0.0.1:9101/ws/ticks > /tmp/dex-ws.log 2>&1 &
    WS_DEX_PID=$!
else
    (echo "" | websocat ws://127.0.0.1:9101/ws/ticks > /tmp/dex-ws.log 2>&1) &
    WS_DEX_PID=$!
fi
sleep 3
kill $WS_DEX_PID 2>/dev/null || true

if grep -q "subscribed" /tmp/dex-ws.log || grep -q "price" /tmp/dex-ws.log; then
    pass "DEX WebSocket connection successful"
else
    fail "DEX WebSocket connection failed"
    cat /tmp/dex-ws.log
fi

echo "Testing Oracle WebSocket..."
if command -v gtimeout &> /dev/null; then
    echo "" | gtimeout 5s websocat ws://127.0.0.1:9102/ws/prices > /tmp/oracle-ws.log 2>&1 &
    WS_ORACLE_PID=$!
elif command -v timeout &> /dev/null; then
    echo "" | timeout 5s websocat ws://127.0.0.1:9102/ws/prices > /tmp/oracle-ws.log 2>&1 &
    WS_ORACLE_PID=$!
else
    (echo "" | websocat ws://127.0.0.1:9102/ws/prices > /tmp/oracle-ws.log 2>&1) &
    WS_ORACLE_PID=$!
fi
sleep 3
kill $WS_ORACLE_PID 2>/dev/null || true

if grep -q "subscribed" /tmp/oracle-ws.log || grep -q "price" /tmp/oracle-ws.log; then
    pass "Oracle WebSocket connection successful"
else
    fail "Oracle WebSocket connection failed"
    cat /tmp/oracle-ws.log
fi

echo ""
echo "Cleaning up..."
kill $DEX_PID 2>/dev/null || true
kill $ORACLE_PID 2>/dev/null || true
sleep 1 2>/dev/null

echo ""
echo "Test Results:"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi

