#pragma once

#include "DeliverySystem.h"
#include "DynamicArray.h"

#include <string>

class Graph;

struct DeliveryStatusMetric {
    std::string status;
    int count = 0;
    double percentage = 0.0;
};

struct TrafficMetric {
    std::string level;
    int roadCount = 0;
    double distanceKm = 0.0;
    double percentage = 0.0;
};

struct RiderPerformance {
    std::string riderId;
    std::string riderName;
    std::string vehicle;
    int completedDeliveries = 0;
    double earnings = 0.0;
    double rating = 0.0;
    bool available = false;
    double performanceScore = 0.0;

    bool operator<(const RiderPerformance& other) const;
};

struct NetworkHealthMetrics {
    int totalLocations = 0;
    int reachableLocations = 0;
    int totalRoads = 0;
    int openRoads = 0;
    int blockedRoads = 0;
    double totalRoadDistanceKm = 0.0;
    double reachablePercentage = 0.0;
    double openRoadPercentage = 0.0;
    bool fullyConnected = false;
};

struct FleetMetrics {
    int totalRiders = 0;
    int ratedRiders = 0;
    int availableRiders = 0;
    int busyRiders = 0;
    int completedDeliveries = 0;
    double totalEarnings = 0.0;
    double averageRating = 0.0;
    double availabilityPercentage = 0.0;
};

struct FinancialMetrics {
    double grossRevenue = 0.0;
    double riderPayouts = 0.0;
    double platformProfit = 0.0;
    double averageCompletedFare = 0.0;
    double profitMarginPercentage = 0.0;
};

struct AnalyticsSnapshot {
    long long generatedAtEpoch = 0;
    DashboardSummary dashboard;
    NetworkHealthMetrics network;
    FleetMetrics fleet;
    FinancialMetrics finance;
    DynamicArray<DeliveryStatusMetric> statusDistribution;
    DynamicArray<TrafficMetric> trafficDistribution;
    DynamicArray<RiderPerformance> riderLeaderboard;
};

class AnalyticsService {
public:
    AnalyticsService();

    bool initialize(const DeliverySystem* deliverySystem, const Graph* graph);
    bool ready() const;
    AnalyticsSnapshot createSnapshot(int leaderboardLimit = 5) const;

private:
    const DeliverySystem* deliverySystem_;
    const Graph* graph_;
    bool ready_;

    NetworkHealthMetrics calculateNetworkHealth() const;
    FleetMetrics calculateFleetMetrics() const;
    FinancialMetrics calculateFinancialMetrics(const DashboardSummary& dashboard) const;
    DynamicArray<DeliveryStatusMetric> calculateStatusDistribution() const;
    DynamicArray<TrafficMetric> calculateTrafficDistribution() const;
    DynamicArray<RiderPerformance> calculateRiderLeaderboard(int limit) const;
    int reachableLocationCount() const;
    static double percentage(double part, double whole);
    static long long currentEpochSeconds();
};
