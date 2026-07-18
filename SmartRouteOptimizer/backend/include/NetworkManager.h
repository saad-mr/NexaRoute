#pragma once

#include "Graph.h"

#include <filesystem>
#include <string>

class RouteEngine;

struct AddNamedDestinationRequest {
    std::string locationName;
    std::string locationType;
    int connectFromId = -1;
    std::string routeName;
    double distanceKm = 0.0;
    int baseTimeMinutes = 0;
    TrafficLevel traffic = TrafficLevel::Low;
};

struct AddExistingRoadRequest {
    std::string routeName;
    int fromId = -1;
    int toId = -1;
    double distanceKm = 0.0;
    int baseTimeMinutes = 0;
    TrafficLevel traffic = TrafficLevel::Low;
};

struct NetworkChangeResult {
    bool success = false;
    std::string error;
    int locationId = -1;
    std::string roadId;
};

class NetworkManager {
public:
    NetworkManager();

    bool initialize(
        RouteEngine* routeEngine,
        const std::filesystem::path& runtimeDirectory,
        bool persistenceEnabled
    );
    bool ready() const;

    NetworkChangeResult addNamedDestination(const AddNamedDestinationRequest& request);
    NetworkChangeResult addRoad(const AddExistingRoadRequest& request);
    int resolveLocationId(const std::string& idOrName) const;
    std::string nextRoadIdPreview() const;
    int customLocationCount() const;
    int customRoadCount() const;

private:
    struct SuggestedPosition {
        int x = 0;
        int y = 0;
    };

    RouteEngine* routeEngine_;
    std::filesystem::path runtimeDirectory_;
    bool persistenceEnabled_;
    bool ready_;
    int roadSequence_;
    int customLocationCount_;
    int customRoadCount_;

    bool loadLocations();
    bool loadRoads();
    SuggestedPosition suggestPosition(int anchorId) const;
    bool positionIsAvailable(int x, int y, int minimumDistance) const;
    std::string nextRoadId();
    bool validateNamedDestination(
        const AddNamedDestinationRequest& request,
        std::string& error
    ) const;
    bool validateExistingRoad(
        const AddExistingRoadRequest& request,
        std::string& error
    ) const;
    void persistLocation(const Location& location) const;
    void persistRoad(
        const std::string& roadId,
        const std::string& roadName,
        int from,
        int to,
        double distance,
        int baseTime,
        TrafficLevel traffic
    ) const;
    void appendAudit(
        const std::string& operation,
        const std::string& identity,
        bool success,
        const std::string& detail
    ) const;
    static std::string safeField(const std::string& value);
    static long long currentEpochSeconds();
};
