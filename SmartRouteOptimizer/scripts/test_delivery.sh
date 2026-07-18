#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    tests/delivery_system_tests.cpp \
    backend/src/Graph.cpp \
    backend/src/RouteEngine.cpp \
    backend/src/DeliverySystem.cpp \
    backend/src/Validation.cpp \
    -Ibackend/include \
    -o build/delivery_system_tests

./build/delivery_system_tests
