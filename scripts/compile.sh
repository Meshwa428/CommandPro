#!/bin/bash
mkdir -p build
cd build || exit

PROFILE_FLAG=""
if [[ "$1" == "--profile" ]]; then
    PROFILE_FLAG="-DSYNAPSE_PROFILER=ON"
    echo "🔨 Building with Profiler Enabled..."
fi

cmake -DCMAKE_BUILD_TYPE=Release $PROFILE_FLAG ..
make -j$(nproc)
cd .. || exit

if [[ "$1" == "--profile" ]]; then
    export SYNAPSE_PROFILE=1
fi

./scripts/verify.sh
