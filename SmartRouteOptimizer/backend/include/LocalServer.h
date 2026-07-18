#pragma once

#include "AnalyticsService.h"
#include "AuthService.h"
#include "DeliverySystem.h"
#include "NetworkManager.h"
#include "RouteEngine.h"
#include "Stack.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

class LocalServer {
public:
    LocalServer(
        std::filesystem::path projectRoot,
        unsigned short port,
        bool persistenceEnabled = true,
        bool automationEnabled = true
    );
    ~LocalServer();

    LocalServer(const LocalServer&) = delete;
    LocalServer& operator=(const LocalServer&) = delete;

    bool start();
    void stop();

    unsigned short port() const;
    const std::filesystem::path& projectRoot() const;

private:
    struct RoadChange {
        std::string roadId;
        bool previousBlocked = false;
        TrafficLevel previousTraffic = TrafficLevel::Low;
    };

    struct HttpRequest {
        std::string method;
        std::string target;
        std::string body;
    };

    std::filesystem::path projectRoot_;
    std::filesystem::path runtimeDirectory_;
    unsigned short port_;
    bool persistenceEnabled_;
    bool automationEnabled_;
    std::atomic<bool> running_;
    std::intptr_t listenSocket_;
    RouteEngine routeEngine_;
    NetworkManager networkManager_;
    bool routeEngineReady_;
    DeliverySystem deliverySystem_;
    AuthService authService_;
    AnalyticsService analyticsService_;
    Stack<RoadChange> roadChangeHistory_;

    bool initializeNetwork();
    void cleanupNetwork();
    void handleClient(std::intptr_t clientSocket);

    HttpRequest readRequest(std::intptr_t clientSocket) const;
    std::string routeRequest(const HttpRequest& request, int& statusCode, std::string& contentType);
    std::string serveStaticFile(const std::string& target, int& statusCode, std::string& contentType) const;
    std::string buildCityJson() const;
    std::string buildRiderJson(const Rider& rider, bool includePrivateDetails = true) const;
    std::string buildRidersJson(bool includePrivateDetails = false) const;
    std::string buildRouteComparisonJson(int from, int to) const;
    std::string buildBookingJson(const Booking& booking) const;
    std::string buildBookingsJson() const;
    std::string buildDashboardJson(bool includeFinancialDetails = false) const;
    std::string buildAnalyticsJson() const;
    std::string buildSessionJson(const AuthResult& result) const;
    std::string requestToken(const HttpRequest& request) const;
    bool authorize(
        const HttpRequest& request,
        AccountRole role,
        int& statusCode,
        std::string& errorJson,
        const std::string& requiredAccountId = {}
    );

    static std::string makeHttpResponse(int statusCode, const std::string& contentType, const std::string& body);
    static std::string mimeTypeFor(const std::filesystem::path& path);
    static std::string jsonEscape(const std::string& value);
};
