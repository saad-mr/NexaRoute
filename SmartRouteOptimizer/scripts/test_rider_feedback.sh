#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    tests/rider_feedback_tests.cpp \
    backend/src/Graph.cpp \
    backend/src/RouteEngine.cpp \
    backend/src/DeliverySystem.cpp \
    backend/src/Validation.cpp \
    -Ibackend/include \
    -o build/rider_feedback_tests

./build/rider_feedback_tests
