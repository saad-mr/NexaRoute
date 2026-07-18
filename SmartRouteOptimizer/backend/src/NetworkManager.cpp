#include "NetworkManager.h"

#include "RouteEngine.h"
#include "Validation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr double PI = 3.14159265358979323846;

struct ParsedLine {
    std::string values[8];
    int count = 0;
};

ParsedLine splitLine(const std::string& line) {
    ParsedLine result;
    std::stringstream stream(line);
    while (result.count < 8 && std::getline(stream, result.values[result.count], '\t')) {
        ++result.count;
    }
    return result;
}

int clampCoordinate(int value, int minimum, int maximum) {
    return std::max(minimum, std::min(maximum, value));
}
}

NetworkManager::NetworkManager()
    : routeEngine_(nullptr),
      runtimeDirectory_(),
      persistenceEnabled_(false),
      ready_(false),
      roadSequence_(0),
      customLocationCount_(0),
      customRoadCount_(0) {
}

bool NetworkManager::initialize(
    RouteEngine* routeEngine,
    const std::filesystem::path& runtimeDirectory,
    bool persistenceEnabled
) {
    routeEngine_ = routeEngine;
    runtimeDirectory_ = runtimeDirectory;
    persistenceEnabled_ = persistenceEnabled && !runtimeDirectory.empty();
    ready_ = routeEngine_ != nullptr && routeEngine_->graph().vertexCount() > 0;
    roadSequence_ = 0;
    customLocationCount_ = 0;
    customRoadCount_ = 0;
    if (!ready_) return false;

    if (persistenceEnabled_) {
        std::error_code error;
        std::filesystem::create_directories(runtimeDirectory_, error);
        if (error) persistenceEnabled_ = false;
    }
    if (persistenceEnabled_) {
        loadLocations();
        loadRoads();
    }
    appendAudit("service_start", "network", true, "Custom network manager initialized");
    return true;
}

bool NetworkManager::ready() const {
    return ready_;
}

NetworkChangeResult NetworkManager::addNamedDestination(
    const AddNamedDestinationRequest& request
) {
    NetworkChangeResult result;
    if (!ready_) {
        result.error = "The city network is unavailable.";
        return result;
    }
    if (!validateNamedDestination(request, result.error)) {
        appendAudit("add_destination", request.locationName, false, result.error);
        return result;
    }

    const SuggestedPosition position = suggestPosition(request.connectFromId);
    const std::string cleanLocationName = Validation::collapseSpaces(Validation::trim(request.locationName));
    const std::string cleanLocationType = Validation::collapseSpaces(Validation::trim(request.locationType));
    const std::string cleanRouteName = Validation::collapseSpaces(Validation::trim(request.routeName));
    const std::string roadId = nextRoadId();

    const int locationId = routeEngine_->graph().addLocation(
        cleanLocationName,
        cleanLocationType,
        position.x,
        position.y
    );
    if (locationId < 0) {
        result.error = "The named destination could not be placed on the map.";
        appendAudit("add_destination", cleanLocationName, false, result.error);
        return result;
    }
    if (!routeEngine_->graph().addRoad(
            roadId,
            cleanRouteName,
            request.connectFromId,
            locationId,
            request.distanceKm,
            request.baseTimeMinutes,
            request.traffic)) {
        result.error = "The connecting route could not be created.";
        appendAudit("add_destination", cleanLocationName, false, result.error);
        return result;
    }

    const Location* location = routeEngine_->graph().location(locationId);
    if (location != nullptr) persistLocation(*location);
    persistRoad(
        roadId,
        cleanRouteName,
        request.connectFromId,
        locationId,
        request.distanceKm,
        request.baseTimeMinutes,
        request.traffic
    );
    ++customLocationCount_;
    ++customRoadCount_;
    result.success = true;
    result.locationId = locationId;
    result.roadId = roadId;
    appendAudit("add_destination", cleanLocationName, true, roadId + " | " + cleanRouteName);
    return result;
}

NetworkChangeResult NetworkManager::addRoad(const AddExistingRoadRequest& request) {
    NetworkChangeResult result;
    if (!ready_) {
        result.error = "The city network is unavailable.";
        return result;
    }
    if (!validateExistingRoad(request, result.error)) {
        appendAudit("add_route", request.routeName, false, result.error);
        return result;
    }

    const std::string routeName = Validation::collapseSpaces(Validation::trim(request.routeName));
    const std::string roadId = nextRoadId();
    if (!routeEngine_->graph().addRoad(
            roadId,
            routeName,
            request.fromId,
            request.toId,
            request.distanceKm,
            request.baseTimeMinutes,
            request.traffic)) {
        result.error = "The named route could not be added.";
        appendAudit("add_route", routeName, false, result.error);
        return result;
    }

    persistRoad(
        roadId,
        routeName,
        request.fromId,
        request.toId,
        request.distanceKm,
        request.baseTimeMinutes,
        request.traffic
    );
    ++customRoadCount_;
    result.success = true;
    result.roadId = roadId;
    appendAudit("add_route", routeName, true, roadId);
    return result;
}

int NetworkManager::resolveLocationId(const std::string& idOrName) const {
    if (!ready_) return -1;
    const std::string cleaned = Validation::collapseSpaces(Validation::trim(idOrName));
    if (cleaned.empty()) return -1;
    try {
        std::size_t parsed = 0;
        const int id = std::stoi(cleaned, &parsed);
        if (parsed == cleaned.size() && routeEngine_->graph().validVertex(id)) return id;
    } catch (...) {
        // A non-numeric value is resolved as a saved location name.
    }
    const Location* location = routeEngine_->graph().locationByName(cleaned);
    return location == nullptr ? -1 : location->id;
}

std::string NetworkManager::nextRoadIdPreview() const {
    std::ostringstream value;
    value << "NR-" << std::setw(3) << std::setfill('0') << (roadSequence_ + 1);
    return value.str();
}

int NetworkManager::customLocationCount() const {
    return customLocationCount_;
}

int NetworkManager::customRoadCount() const {
    return customRoadCount_;
}

bool NetworkManager::loadLocations() {
    std::ifstream input(runtimeDirectory_ / "added_locations.db");
    if (!input) return false;
    std::string line;
    while (std::getline(input, line)) {
        const ParsedLine fields = splitLine(line);
        if (fields.count != 5) continue;
        try {
            const int expectedId = std::stoi(fields.values[0]);
            const int x = std::stoi(fields.values[3]);
            const int y = std::stoi(fields.values[4]);
            if (expectedId != routeEngine_->graph().vertexCount()) continue;
            const int created = routeEngine_->graph().addLocation(
                fields.values[1],
                fields.values[2],
                x,
                y
            );
            if (created == expectedId) ++customLocationCount_;
        } catch (...) {
            // Keep every valid custom location even when one saved row is malformed.
        }
    }
    return true;
}

bool NetworkManager::loadRoads() {
    std::ifstream input(runtimeDirectory_ / "added_roads.db");
    if (!input) return false;
    std::string line;
    while (std::getline(input, line)) {
        const ParsedLine fields = splitLine(line);
        if (fields.count != 6 && fields.count != 7) continue;
        try {
            const bool legacy = fields.count == 6;
            const std::string roadId = fields.values[0];
            const std::string roadName = legacy ? roadId : fields.values[1];
            const int offset = legacy ? 0 : 1;
            const int from = std::stoi(fields.values[1 + offset]);
            const int to = std::stoi(fields.values[2 + offset]);
            const double distance = std::stod(fields.values[3 + offset]);
            const int baseTime = std::stoi(fields.values[4 + offset]);
            const TrafficLevel traffic = Graph::parseTrafficLevel(fields.values[5 + offset]);
            if (routeEngine_->graph().addRoad(roadId, roadName, from, to, distance, baseTime, traffic)) {
                ++customRoadCount_;
            }
            if (roadId.rfind("NR-", 0) == 0) {
                const int sequence = std::stoi(roadId.substr(3));
                if (sequence > roadSequence_) roadSequence_ = sequence;
            }
        } catch (...) {
            // Keep every valid saved route even when one row is malformed.
        }
    }
    return true;
}

NetworkManager::SuggestedPosition NetworkManager::suggestPosition(int anchorId) const {
    const Location* anchor = routeEngine_->graph().location(anchorId);
    const int centerX = anchor == nullptr ? 600 : anchor->x;
    const int centerY = anchor == nullptr ? 350 : anchor->y;
    const int sequence = routeEngine_->graph().vertexCount();

    for (int attempt = 0; attempt < 30; ++attempt) {
        const double angle = (sequence * 137.508 + attempt * 47.0) * PI / 180.0;
        const int radius = 115 + (attempt % 4) * 38;
        const int x = clampCoordinate(
            centerX + static_cast<int>(std::round(std::cos(angle) * radius)),
            70,
            1130
        );
        const int y = clampCoordinate(
            centerY + static_cast<int>(std::round(std::sin(angle) * radius * 0.72)),
            60,
            650
        );
        if (positionIsAvailable(x, y, 58)) return {x, y};
    }

    const int fallbackX = 90 + (sequence * 83) % 1020;
    const int fallbackY = 80 + (sequence * 67) % 540;
    return {fallbackX, fallbackY};
}

bool NetworkManager::positionIsAvailable(
    int x,
    int y,
    int minimumDistance
) const {
    const Graph& graph = routeEngine_->graph();
    for (int id = 0; id < graph.vertexCount(); ++id) {
        const Location* location = graph.location(id);
        if (location == nullptr) continue;
        const double distance = std::hypot(
            static_cast<double>(location->x - x),
            static_cast<double>(location->y - y)
        );
        if (distance < minimumDistance) return false;
    }
    return true;
}

std::string NetworkManager::nextRoadId() {
    std::string candidate;
    do {
        ++roadSequence_;
        std::ostringstream value;
        value << "NR-" << std::setw(3) << std::setfill('0') << roadSequence_;
        candidate = value.str();
    } while (routeEngine_->graph().roadById(candidate) != nullptr);
    return candidate;
}

bool NetworkManager::validateNamedDestination(
    const AddNamedDestinationRequest& request,
    std::string& error
) const {
    const Graph& graph = routeEngine_->graph();
    if (!Validation::isLocationName(request.locationName)) {
        error = "Enter a destination name containing 2 to 60 valid characters.";
        return false;
    }
    if (graph.locationByName(request.locationName) != nullptr) {
        error = "A saved location already uses this name.";
        return false;
    }
    if (!Validation::isLocationType(request.locationType)) {
        error = "Enter a valid destination type.";
        return false;
    }
    if (!graph.validVertex(request.connectFromId)) {
        error = "Choose an existing location to connect from.";
        return false;
    }
    if (!Validation::isRouteName(request.routeName)) {
        error = "Enter a route name containing 2 to 60 valid characters.";
        return false;
    }
    if (!Validation::isDistance(request.distanceKm)) {
        error = "Route distance must be between 0.1 and 100 kilometres.";
        return false;
    }
    if (!Validation::isTravelTime(request.baseTimeMinutes)) {
        error = "Travel time must be between 1 and 180 minutes.";
        return false;
    }
    return true;
}

bool NetworkManager::validateExistingRoad(
    const AddExistingRoadRequest& request,
    std::string& error
) const {
    const Graph& graph = routeEngine_->graph();
    if (!Validation::isRouteName(request.routeName)) {
        error = "Enter a route name containing 2 to 60 valid characters.";
        return false;
    }
    if (!graph.validVertex(request.fromId) || !graph.validVertex(request.toId) ||
        request.fromId == request.toId) {
        error = "Choose two different saved locations.";
        return false;
    }
    if (graph.edgeBetween(request.fromId, request.toId) != nullptr) {
        error = "These locations already have a direct route.";
        return false;
    }
    if (!Validation::isDistance(request.distanceKm)) {
        error = "Route distance must be between 0.1 and 100 kilometres.";
        return false;
    }
    if (!Validation::isTravelTime(request.baseTimeMinutes)) {
        error = "Travel time must be between 1 and 180 minutes.";
        return false;
    }
    return true;
}

void NetworkManager::persistLocation(const Location& location) const {
    if (!persistenceEnabled_) return;
    std::ofstream output(runtimeDirectory_ / "added_locations.db", std::ios::app);
    if (!output) return;
    output << location.id << '\t'
           << safeField(location.name) << '\t'
           << safeField(location.type) << '\t'
           << location.x << '\t'
           << location.y << '\n';
}

void NetworkManager::persistRoad(
    const std::string& roadId,
    const std::string& roadName,
    int from,
    int to,
    double distance,
    int baseTime,
    TrafficLevel traffic
) const {
    if (!persistenceEnabled_) return;
    std::ofstream output(runtimeDirectory_ / "added_roads.db", std::ios::app);
    if (!output) return;
    output << safeField(roadId) << '\t'
           << safeField(roadName) << '\t'
           << from << '\t'
           << to << '\t'
           << std::setprecision(12) << distance << '\t'
           << baseTime << '\t'
           << Graph::trafficName(traffic) << '\n';
}

void NetworkManager::appendAudit(
    const std::string& operation,
    const std::string& identity,
    bool success,
    const std::string& detail
) const {
    if (!persistenceEnabled_) return;
    std::ofstream output(runtimeDirectory_ / "network_audit.db", std::ios::app);
    if (!output) return;
    output << currentEpochSeconds() << '\t'
           << safeField(operation) << '\t'
           << safeField(identity) << '\t'
           << (success ? 1 : 0) << '\t'
           << safeField(detail) << '\n';
}

std::string NetworkManager::safeField(const std::string& value) {
    std::string safe = value;
    for (char& character : safe) {
        if (character == '\t' || character == '\r' || character == '\n') character = ' ';
    }
    return safe;
}

long long NetworkManager::currentEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
