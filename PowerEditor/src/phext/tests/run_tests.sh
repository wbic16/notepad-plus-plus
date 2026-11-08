#!/bin/sh
# Build and run the standalone phext core tests (no Windows deps).
# Usage: ./run_tests.sh [corpus.phext ...]
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra -I.. test_phext.cpp \
    ../PhextDocument.cpp ../SubspaceSpec.cpp ../Projection.cpp -o test_phext
./test_phext "$@"
