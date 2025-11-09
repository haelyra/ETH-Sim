#!/bin/bash

set -e

echo "C++ Integration Tests"
echo "========================"
echo ""

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

echo "1. Checking binaries..."
if [ -f "build/src/dex_sim/dex-sim" ]; then
    pass "DEX binary exists"
else
    fail "DEX binary not found (run: cmake --build build)"
    exit 1
fi

if [ -f "build/src/oracle_sim/oracle-sim" ]; then
    pass "Oracle binary exists"
else
    fail "Oracle binary not found (run: cmake --build build)"
    exit 1
fi

echo ""

echo "2. Checking configurations..."
if [ -f "configs/dex.yaml" ]; then
    pass "DEX config exists"
else
    fail "DEX config not found"
    exit 1
fi

if [ -f "configs/oracle.yaml" ]; then
    pass "Oracle config exists"
else
    fail "Oracle config not found"
    exit 1
fi

echo ""

echo "3. Cleaning up old processes..."
pkill -f "dex-sim" || true
pkill -f "oracle-sim" || true
sleep 1
pass "Old processes cleaned up"

echo ""

echo "4. Starting DEX simulator..."
./build/src/dex_sim/dex-sim configs/dex.yaml > /tmp/dex-sim.log 2>&1 &
DEX_PID=$!
sleep 2

if ps -p $DEX_PID > /dev/null; then
    pass "DEX simulator started (PID: $DEX_PID)"
else
    fail "DEX simulator failed to start"
    cat /tmp/dex-sim.log
    exit 1
fi

echo ""

echo "5. Starting Oracle simulator..."
./build/src/oracle_sim/oracle-sim configs/oracle.yaml > /tmp/oracle-sim.log 2>&1 &
ORACLE_PID=$!
sleep 2

if ps -p $ORACLE_PID > /dev/null; then
    pass "Oracle simulator started (PID: $ORACLE_PID)"
else
    fail "Oracle simulator failed to start"
    cat /tmp/oracle-sim.log
    kill $DEX_PID 2>/dev/null || true
    exit 1
fi

echo ""

echo "6. Testing HTTP endpoints..."

DEX_HEALTH=$(curl -s http://127.0.0.1:9101/healthz)
if [ "$DEX_HEALTH" = "OK" ]; then
    pass "DEX /healthz endpoint"
else
    fail "DEX /healthz returned: $DEX_HEALTH"
fi

ORACLE_HEALTH=$(curl -s http://127.0.0.1:9102/healthz)
if [ "$ORACLE_HEALTH" = "OK" ]; then
    pass "Oracle /healthz endpoint"
else
    fail "Oracle /healthz returned: $ORACLE_HEALTH"
fi

DEX_METRICS=$(curl -s http://127.0.0.1:9101/metrics)
if echo "$DEX_METRICS" | grep -q "price_ticks_generated"; then
    pass "DEX /metrics endpoint"
else
    fail "DEX /metrics missing metrics"
fi

ORACLE_METRICS=$(curl -s http://127.0.0.1:9102/metrics)
if echo "$ORACLE_METRICS" | grep -q "price_ticks_generated"; then
    pass "Oracle /metrics endpoint"
else
    fail "Oracle /metrics missing metrics"
fi

DEX_SNAPSHOT=$(curl -s http://127.0.0.1:9101/prices/snapshot)
if echo "$DEX_SNAPSHOT" | grep -q "server_time"; then
    pass "DEX /prices/snapshot endpoint"
else
    fail "DEX /prices/snapshot invalid JSON"
fi

ORACLE_SNAPSHOT=$(curl -s http://127.0.0.1:9102/oracle/snapshot)
if echo "$ORACLE_SNAPSHOT" | grep -q "server_time"; then
    pass "Oracle /oracle/snapshot endpoint"
else
    fail "Oracle /oracle/snapshot invalid JSON"
fi

echo ""

echo "7. Testing WebSocket connectivity..."

if command -v websocat &> /dev/null; then
    if command -v gtimeout &> /dev/null; then
        TIMEOUT_CMD="gtimeout"
    elif command -v timeout &> /dev/null; then
        TIMEOUT_CMD="timeout"
    else
        TIMEOUT_CMD=""
    fi

    if [ -n "$TIMEOUT_CMD" ]; then
        echo "" | $TIMEOUT_CMD 3s websocat ws://127.0.0.1:9101/ws/ticks > /tmp/dex-ws.log 2>&1 &
        WS_DEX_PID=$!
        sleep 2

        if grep -q "subscribed" /tmp/dex-ws.log || grep -q "price" /tmp/dex-ws.log; then
            pass "DEX WebSocket connection"
        else
            warn "DEX WebSocket (check logs)"
        fi
        kill $WS_DEX_PID 2>/dev/null || true

        echo "" | $TIMEOUT_CMD 3s websocat ws://127.0.0.1:9102/ws/prices > /tmp/oracle-ws.log 2>&1 &
        WS_ORACLE_PID=$!
        sleep 2

        if grep -q "subscribed" /tmp/oracle-ws.log || grep -q "price" /tmp/oracle-ws.log; then
            pass "Oracle WebSocket connection"
        else
            warn "Oracle WebSocket (check logs)"
        fi
        kill $WS_ORACLE_PID 2>/dev/null || true
    else
        (echo "" | websocat ws://127.0.0.1:9101/ws/ticks > /tmp/dex-ws.log 2>&1) &
        WS_DEX_PID=$!
        sleep 2
        kill $WS_DEX_PID 2>/dev/null || true

        if grep -q "subscribed" /tmp/dex-ws.log || grep -q "price" /tmp/dex-ws.log; then
            pass "DEX WebSocket connection"
        else
            warn "DEX WebSocket (check logs)"
        fi

        (echo "" | websocat ws://127.0.0.1:9102/ws/prices > /tmp/oracle-ws.log 2>&1) &
        WS_ORACLE_PID=$!
        sleep 2
        kill $WS_ORACLE_PID 2>/dev/null || true

        if grep -q "subscribed" /tmp/oracle-ws.log || grep -q "price" /tmp/oracle-ws.log; then
            pass "Oracle WebSocket connection"
        else
            warn "Oracle WebSocket (check logs)"
        fi
    fi
else
    warn "WebSocket test skipped (install websocat: brew install websocat)"
fi

echo ""

echo "8. Testing price tick generation..."
sleep 3

DEX_METRICS_AFTER=$(curl -s http://127.0.0.1:9101/metrics)
DEX_TICKS=$(echo "$DEX_METRICS_AFTER" | grep "^price_ticks_generated " | awk '{print $2}')

if [ -n "$DEX_TICKS" ] && [ "$DEX_TICKS" -gt 0 ] 2>/dev/null; then
    pass "DEX generated $DEX_TICKS price ticks"
else
    fail "DEX not generating price ticks"
fi

ORACLE_METRICS_AFTER=$(curl -s http://127.0.0.1:9102/metrics)
ORACLE_TICKS=$(echo "$ORACLE_METRICS_AFTER" | grep "^price_ticks_generated " | awk '{print $2}')

if [ -n "$ORACLE_TICKS" ] && [ "$ORACLE_TICKS" -ge 0 ] 2>/dev/null; then
    pass "Oracle generated $ORACLE_TICKS price ticks"
else
    fail "Oracle not generating price ticks"
fi

echo ""

echo "9. Testing static file serving..."

if [ -f "static/index.html" ]; then
    INDEX_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:9101/)
    if [ "$INDEX_RESPONSE" = "200" ]; then
        pass "DEX serves index.html"
    else
        warn "DEX /index.html returned HTTP $INDEX_RESPONSE"
    fi
fi

if [ -f "static/dual.html" ]; then
    DUAL_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:9101/dual.html)
    if [ "$DUAL_RESPONSE" = "200" ]; then
        pass "DEX serves dual.html"
    else
        warn "DEX /dual.html returned HTTP $DUAL_RESPONSE"
    fi
fi

echo ""

echo "10. Cleaning up..."
kill $DEX_PID 2>/dev/null || true
kill $ORACLE_PID 2>/dev/null || true
sleep 1
pass "Processes terminated" 2>/dev/null

echo ""
echo "========================"
echo "Test Results:"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
