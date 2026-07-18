#include "AnalyticsService.h"

#include "CircularQueue.h"
#include "Graph.h"
#include "MinHeap.h"

#include <chrono>
#include <cmath>

namespace {
constexpr int DELIVERY_STATUS_COUNT = 9;

const DeliveryStatus ALL_STATUSES[DELIVERY_STATUS_COUNT] = {
    DeliveryStatus::Requested,
    DeliveryStatus::SearchingForRider,
    DeliveryStatus::OfferReceived,
    DeliveryStatus::RiderAssigned,
    DeliveryStatus::RiderArriving,
    DeliveryStatus::PickedUp,
    DeliveryStatus::InTransit,
    DeliveryStatus::Delivered,
    DeliveryStatus::Cancelled
};

int statusIndex(DeliveryStatus status) {
    for (int index = 0; index < DELIVERY_STATUS_COUNT; ++index) {
        if (ALL_STATUSES[index] == status) return index;
    }
    return -1;
}

int trafficIndex(TrafficLevel level) {
    if (level == TrafficLevel::Medium) return 1;
    if (level == TrafficLevel::High) return 2;
    return 0;
}

double riderScore(const Rider& rider) {
    const double experienceScore = std::log1p(
        static_cast<double>(rider.completedDeliveries)
    ) * 11.0;
    const double ratingScore = rider.ratingCount > 0 ? rider.rating * 13.0 : 0.0;
    const double reliabilityScore = rider.available ? 5.0 : 2.0;
    return ratingScore + experienceScore + reliabilityScore;
}
}

bool RiderPerformance::operator<(const RiderPerformance& other) const {
    if (std::fabs(performanceScore - other.performanceScore) > 0.0001) {
        return performanceScore > other.performanceScore;
    }
    if (completedDeliveries != other.completedDeliveries) {
        return completedDeliveries > other.completedDeliveries;
    }
    return riderId < other.riderId;
}

AnalyticsService::AnalyticsService()
    : deliverySystem_(nullptr), graph_(nullptr), ready_(false) {
}

bool AnalyticsService::initialize(
    const DeliverySystem* deliverySystem,
    const Graph* graph
) {
    deliverySystem_ = deliverySystem;
    graph_ = graph;
    ready_ = deliverySystem_ != nullptr && graph_ != nullptr &&
             deliverySystem_->ready() && graph_->vertexCount() > 0;
    return ready_;
}

bool AnalyticsService::ready() const {
    return ready_;
}

AnalyticsSnapshot AnalyticsService::createSnapshot(int leaderboardLimit) const {
    AnalyticsSnapshot snapshot;
    snapshot.generatedAtEpoch = currentEpochSeconds();
    if (!ready_) return snapshot;

    snapshot.dashboard = deliverySystem_->dashboardSummary();
    snapshot.network = calculateNetworkHealth();
    snapshot.fleet = calculateFleetMetrics();
    snapshot.finance = calculateFinancialMetrics(snapshot.dashboard);
    snapshot.statusDistribution = calculateStatusDistribution();
    snapshot.trafficDistribution = calculateTrafficDistribution();
    snapshot.riderLeaderboard = calculateRiderLeaderboard(leaderboardLimit);
    return snapshot;
}

NetworkHealthMetrics AnalyticsService::calculateNetworkHealth() const {
    NetworkHealthMetrics metrics;
    if (!ready_) return metrics;

    metrics.totalLocations = graph_->vertexCount();
    metrics.totalRoads = graph_->roadCount();
    metrics.blockedRoads = graph_->blockedRoadCount();
    metrics.openRoads = metrics.totalRoads - metrics.blockedRoads;
    metrics.reachableLocations = reachableLocationCount();

    for (int from = 0; from < graph_->vertexCount(); ++from) {
        for (const EdgeNode* edge = graph_->edgesFrom(from);
             edge != nullptr;
             edge = edge->next) {
            if (from < edge->destination) {
                metrics.totalRoadDistanceKm += edge->distanceKm;
            }
        }
    }

    metrics.reachablePercentage = percentage(
        metrics.reachableLocations,
        metrics.totalLocations
    );
    metrics.openRoadPercentage = percentage(
        metrics.openRoads,
        metrics.totalRoads
    );
    metrics.fullyConnected = metrics.totalLocations > 0 &&
                             metrics.reachableLocations == metrics.totalLocations;
    return metrics;
}

FleetMetrics AnalyticsService::calculateFleetMetrics() const {
    FleetMetrics metrics;
    if (!ready_) return metrics;

    const DynamicArray<Rider>& riders = deliverySystem_->riders();
    metrics.totalRiders = static_cast<int>(riders.size());
    double ratingTotal = 0.0;
    for (std::size_t index = 0; index < riders.size(); ++index) {
        const Rider& rider = riders[index];
        if (rider.available) ++metrics.availableRiders;
        else ++metrics.busyRiders;
        metrics.completedDeliveries += rider.completedDeliveries;
        metrics.totalEarnings += rider.earnings;
        if (rider.ratingCount > 0) {
            ratingTotal += rider.rating;
            ++metrics.ratedRiders;
        }
    }
    if (metrics.ratedRiders > 0) {
        metrics.averageRating = ratingTotal / metrics.ratedRiders;
    }
    metrics.availabilityPercentage = percentage(
        metrics.availableRiders,
        metrics.totalRiders
    );
    return metrics;
}

FinancialMetrics AnalyticsService::calculateFinancialMetrics(
    const DashboardSummary& dashboard
) const {
    FinancialMetrics metrics;
    metrics.grossRevenue = dashboard.totalRevenue;
    metrics.riderPayouts = dashboard.riderPayouts;
    metrics.platformProfit = dashboard.platformProfit;
    if (dashboard.deliveredBookings > 0) {
        metrics.averageCompletedFare = dashboard.totalRevenue /
            static_cast<double>(dashboard.deliveredBookings);
    }
    metrics.profitMarginPercentage = percentage(
        dashboard.platformProfit,
        dashboard.totalRevenue
    );
    return metrics;
}

DynamicArray<DeliveryStatusMetric>
AnalyticsService::calculateStatusDistribution() const {
    DynamicArray<DeliveryStatusMetric> metrics(DELIVERY_STATUS_COUNT);
    int counts[DELIVERY_STATUS_COUNT]{};
    int total = 0;
    if (ready_) {
        deliverySystem_->bookings().forEach([&counts, &total](const Booking& booking) {
            const int index = statusIndex(booking.status);
            if (index >= 0) ++counts[index];
            ++total;
        });
    }

    for (int index = 0; index < DELIVERY_STATUS_COUNT; ++index) {
        DeliveryStatusMetric metric;
        metric.status = DeliverySystem::statusName(ALL_STATUSES[index]);
        metric.count = counts[index];
        metric.percentage = percentage(metric.count, total);
        metrics.pushBack(std::move(metric));
    }
    return metrics;
}

DynamicArray<TrafficMetric>
AnalyticsService::calculateTrafficDistribution() const {
    DynamicArray<TrafficMetric> metrics(3);
    const char* names[3] = {"Low", "Medium", "High"};
    int counts[3]{};
    double distances[3]{};
    int totalRoads = 0;

    if (ready_) {
        for (int from = 0; from < graph_->vertexCount(); ++from) {
            for (const EdgeNode* edge = graph_->edgesFrom(from);
                 edge != nullptr;
                 edge = edge->next) {
                if (from >= edge->destination) continue;
                const int index = trafficIndex(edge->traffic);
                ++counts[index];
                distances[index] += edge->distanceKm;
                ++totalRoads;
            }
        }
    }

    for (int index = 0; index < 3; ++index) {
        TrafficMetric metric;
        metric.level = names[index];
        metric.roadCount = counts[index];
        metric.distanceKm = distances[index];
        metric.percentage = percentage(counts[index], totalRoads);
        metrics.pushBack(std::move(metric));
    }
    return metrics;
}

DynamicArray<RiderPerformance>
AnalyticsService::calculateRiderLeaderboard(int limit) const {
    DynamicArray<RiderPerformance> leaderboard;
    if (!ready_ || limit <= 0) return leaderboard;

    MinHeap<RiderPerformance> ranking;
    const DynamicArray<Rider>& riders = deliverySystem_->riders();
    for (std::size_t index = 0; index < riders.size(); ++index) {
        const Rider& rider = riders[index];
        RiderPerformance performance;
        performance.riderId = rider.id;
        performance.riderName = rider.name;
        performance.vehicle = rider.vehicle;
        performance.completedDeliveries = rider.completedDeliveries;
        performance.earnings = rider.earnings;
        performance.rating = rider.rating;
        performance.available = rider.available;
        performance.performanceScore = riderScore(rider);
        ranking.insert(performance);
    }

    RiderPerformance next;
    while (static_cast<int>(leaderboard.size()) < limit &&
           ranking.removeMinimum(next)) {
        leaderboard.pushBack(next);
    }
    return leaderboard;
}

int AnalyticsService::reachableLocationCount() const {
    if (!ready_ || graph_->vertexCount() <= 0) return 0;
    const int count = graph_->vertexCount();
    bool* visited = new bool[static_cast<std::size_t>(count)]{};
    CircularQueue<int> pending(static_cast<std::size_t>(count));
    visited[0] = true;
    pending.enqueue(0);
    int reached = 0;
    int current = -1;

    while (pending.dequeue(current)) {
        ++reached;
        for (const EdgeNode* edge = graph_->edgesFrom(current);
             edge != nullptr;
             edge = edge->next) {
            if (edge->blocked || visited[edge->destination]) continue;
            visited[edge->destination] = true;
            pending.enqueue(edge->destination);
        }
    }

    delete[] visited;
    return reached;
}

double AnalyticsService::percentage(double part, double whole) {
    if (whole <= 0.0) return 0.0;
    return part / whole * 100.0;
}

long long AnalyticsService::currentEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
