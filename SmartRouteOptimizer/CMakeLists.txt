cmake_minimum_required(VERSION 3.20)

project(SmartRouteOptimizer VERSION 3.1.0 LANGUAGES CXX)

add_executable(SmartRouteOptimizer
    backend/src/main.cpp
    backend/src/LocalServer.cpp
    backend/src/Graph.cpp
    backend/src/RouteEngine.cpp
    backend/src/DeliverySystem.cpp
    backend/src/Security.cpp
    backend/src/Validation.cpp
    backend/src/AuthService.cpp
    backend/src/NetworkManager.cpp
    backend/src/AnalyticsService.cpp
)

target_include_directories(SmartRouteOptimizer PRIVATE backend/include)
target_compile_features(SmartRouteOptimizer PRIVATE cxx_std_17)

if(WIN32)
    target_compile_definitions(SmartRouteOptimizer PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
    target_link_libraries(SmartRouteOptimizer PRIVATE ws2_32 shell32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(RouteEngineTests
        tests/route_engine_tests.cpp
        backend/src/Graph.cpp
        backend/src/RouteEngine.cpp
        backend/src/Validation.cpp
    )
    target_include_directories(RouteEngineTests PRIVATE backend/include)
    target_compile_features(RouteEngineTests PRIVATE cxx_std_17)
    add_test(NAME RouteEngineTests COMMAND RouteEngineTests)

    add_executable(DeliverySystemTests
        tests/delivery_system_tests.cpp
        backend/src/Graph.cpp
        backend/src/RouteEngine.cpp
        backend/src/DeliverySystem.cpp
        backend/src/Validation.cpp
    )
    target_include_directories(DeliverySystemTests PRIVATE backend/include)
    target_compile_features(DeliverySystemTests PRIVATE cxx_std_17)
    add_test(NAME DeliverySystemTests COMMAND DeliverySystemTests)

    add_executable(AuthNetworkTests
        tests/auth_network_tests.cpp
        backend/src/Graph.cpp
        backend/src/RouteEngine.cpp
        backend/src/DeliverySystem.cpp
        backend/src/Security.cpp
        backend/src/Validation.cpp
        backend/src/AuthService.cpp
        backend/src/NetworkManager.cpp
        backend/src/AnalyticsService.cpp
    )
    target_include_directories(AuthNetworkTests PRIVATE backend/include)
    target_compile_features(AuthNetworkTests PRIVATE cxx_std_17)
    add_test(NAME AuthNetworkTests COMMAND AuthNetworkTests)

    add_executable(RiderFeedbackTests
        tests/rider_feedback_tests.cpp
        backend/src/Graph.cpp
        backend/src/RouteEngine.cpp
        backend/src/DeliverySystem.cpp
        backend/src/Validation.cpp
    )
    target_include_directories(RiderFeedbackTests PRIVATE backend/include)
    target_compile_features(RiderFeedbackTests PRIVATE cxx_std_17)
    add_test(NAME RiderFeedbackTests COMMAND RiderFeedbackTests)

    set_tests_properties(
        RouteEngineTests
        DeliverySystemTests
        AuthNetworkTests
        RiderFeedbackTests
        PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
