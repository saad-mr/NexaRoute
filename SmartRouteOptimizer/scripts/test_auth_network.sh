#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

g++ -std=c++17 -Wall -Wextra -Wpedantic \
    tests/auth_network_tests.cpp \
    backend/src/Graph.cpp \
    backend/src/RouteEngine.cpp \
    backend/src/DeliverySystem.cpp \
    backend/src/Security.cpp \
    backend/src/Validation.cpp \
    backend/src/AuthService.cpp \
    backend/src/NetworkManager.cpp \
    backend/src/AnalyticsService.cpp \
    -Ibackend/include \
    -o build/auth_network_tests

./build/auth_network_tests
