#!/bin/bash

# Synapse Verification Suite
# Automates the: Compile -> Test -> Benchmark workflow

set -e # Exit on any failure

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}== STEP 1: Compiling Synapse ==${NC}"
mkdir -p build
cd build
cmake .. > /dev/null
make -j$(nproc)
cd ..

echo -e "\n${BLUE}== STEP 2: Running Functional Tests (Pytest) ==${NC}"
if python3 -m pytest tests; then
    echo -e "${GREEN}✅ All tests passed!${NC}"
else
    echo -e "${RED}❌ Functional tests failed! Aborting.${NC}"
    exit 1
fi

echo -e "\n${BLUE}== STEP 3: Running Feature Benchmarks ==${NC}"
# python3 tests/benchmark.py (Retired)

echo -e "\n${BLUE}== STEP 4: Running Runtime Systems Benchmarks ==${NC}"
python3 tests/benchmarks/runner.py

echo -e "\n${GREEN}✨ Verification Complete!${NC}"
