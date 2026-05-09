#!/bin/bash
mkdir -p build
cd build || exit
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd .. || exit
python3 tests/benchmark.py
