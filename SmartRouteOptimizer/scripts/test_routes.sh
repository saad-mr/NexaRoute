#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    tests/route_engine_tests.cpp \
    backend/src/Graph.cpp \
    backend/src/RouteEngine.cpp \
    backend/src/Validation.cpp \
    -Ibackend/include \
    -o build/route_engine_tests

./build/route_engine_tests
