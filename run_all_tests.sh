#!/bin/bash
# Comprehensive test runner for ETH-sim C++

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   ETH-sim C++ Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Track overall results
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0

# Function to run a test suite
run_test_suite() {
    local name=$1
    local command=$2

    echo -e "${BLUE}Running: $name${NC}"
    echo "----------------------------------------"

    if eval "$command"; then
        echo -e "${GREEN}✓ $name PASSED${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${RED}✗ $name FAILED${NC}"
        ((TOTAL_FAILED++))
    fi
    ((TOTAL_TESTS++))
    echo ""
}

# 1. Build the project
echo -e "${YELLOW}Step 1: Building project...${NC}"
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build --parallel || {
    echo -e "${RED}Build failed! Cannot run tests.${NC}"
    exit 1
}
echo -e "${GREEN}Build successful!${NC}"
echo ""

# 2. Run unit tests
echo -e "${YELLOW}Step 2: Unit Tests${NC}"
echo ""

if [ -f "build/tests/test_core" ]; then
    run_test_suite "Core Library Unit Tests" "./build/tests/test_core"
else
    echo -e "${YELLOW}⚠ Unit tests not built (install Google Test: brew install googletest)${NC}"
    echo ""
fi

# 3. Run integration tests
echo -e "${YELLOW}Step 3: Integration Tests${NC}"
echo ""

if [ -f "tests/integration_test.sh" ]; then
    run_test_suite "Integration Tests" "./tests/integration_test.sh"
else
    echo -e "${RED}Integration test script not found${NC}"
    ((TOTAL_FAILED++))
    ((TOTAL_TESTS++))
fi

# 4. Run WebSocket tests (if websocat available)
echo -e "${YELLOW}Step 4: WebSocket Tests${NC}"
echo ""

if command -v websocat &> /dev/null; then
    if [ -f "test-websocket.sh" ]; then
        run_test_suite "WebSocket Connectivity Tests" "./test-websocket.sh"
    else
        echo -e "${YELLOW}⚠ WebSocket test script not found${NC}"
    fi
else
    echo -e "${YELLOW}⚠ WebSocket tests skipped (install: brew install websocat)${NC}"
    echo ""
fi

# 5. Memory leak check (if valgrind available - Linux only)
if command -v valgrind &> /dev/null; then
    echo -e "${YELLOW}Step 5: Memory Leak Tests${NC}"
    echo ""

    # Run DEX for 5 seconds and check for leaks
    echo "Testing DEX simulator for memory leaks..."
    timeout 5s valgrind --leak-check=full --error-exitcode=1 \
        ./build/src/dex_sim/dex-sim configs/dex.yaml \
        > /tmp/valgrind-dex.log 2>&1 || true

    if grep -q "no leaks are possible" /tmp/valgrind-dex.log; then
        echo -e "${GREEN}✓ DEX: No memory leaks${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${YELLOW}⚠ DEX: Possible memory leaks (check /tmp/valgrind-dex.log)${NC}"
        ((TOTAL_FAILED++))
    fi
    ((TOTAL_TESTS++))

    # Run Oracle for 5 seconds and check for leaks
    echo "Testing Oracle simulator for memory leaks..."
    timeout 5s valgrind --leak-check=full --error-exitcode=1 \
        ./build/src/oracle_sim/oracle-sim configs/oracle.yaml \
        > /tmp/valgrind-oracle.log 2>&1 || true

    if grep -q "no leaks are possible" /tmp/valgrind-oracle.log; then
        echo -e "${GREEN}✓ Oracle: No memory leaks${NC}"
        ((TOTAL_PASSED++))
    else
        echo -e "${YELLOW}⚠ Oracle: Possible memory leaks (check /tmp/valgrind-oracle.log)${NC}"
        ((TOTAL_FAILED++))
    fi
    ((TOTAL_TESTS++))
    echo ""
fi

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Total Test Suites: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $TOTAL_PASSED${NC}"

if [ $TOTAL_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TOTAL_FAILED${NC}"
    echo ""
    echo -e "${RED}Some tests failed. Please review the output above.${NC}"
    exit 1
else
    echo ""
    echo -e "${GREEN}✓ All test suites passed!${NC}"
    exit 0
fi
