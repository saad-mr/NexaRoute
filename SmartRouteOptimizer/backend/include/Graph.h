#pragma once

#include "DynamicArray.h"

#include <filesystem>
#include <string>

enum class TrafficLevel {
    Low,
    Medium,
    High
};

struct Location {
    int id = -1;
    std::string name;
    std::string type;
    int x = 0;
    int y = 0;
};

struct EdgeNode {
    std::string roadId;
    std::string roadName;
    int destination = -1;
    double distanceKm = 0.0;
    int baseTimeMinutes = 0;
    TrafficLevel traffic = TrafficLevel::Low;
    bool blocked = false;
    EdgeNode* next = nullptr;
};

class Graph {
public:
    Graph();
    ~Graph();

    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    bool loadFromCsv(const std::filesystem::path& locationsFile, const std::filesystem::path& roadsFile);
    void clear();

    int vertexCount() const;
    int roadCount() const;
    int blockedRoadCount() const;
    bool validVertex(int id) const;

    const Location* location(int id) const;
    const Location* locationByName(const std::string& name) const;
    const EdgeNode* edgesFrom(int id) const;
    const EdgeNode* edgeBetween(int from, int to) const;
    const EdgeNode* roadById(const std::string& roadId) const;

    bool setRoadBlocked(const std::string& roadId, bool blocked);
    bool setRoadTraffic(const std::string& roadId, TrafficLevel level);
    int addLocation(
        const std::string& name,
        const std::string& type,
        int x,
        int y
    );
    bool addRoad(
        const std::string& roadId,
        int from,
        int to,
        double distanceKm,
        int baseTimeMinutes,
        TrafficLevel traffic
    );
    bool addRoad(
        const std::string& roadId,
        const std::string& roadName,
        int from,
        int to,
        double distanceKm,
        int baseTimeMinutes,
        TrafficLevel traffic
    );

    static double trafficMultiplier(TrafficLevel level);
    static TrafficLevel parseTrafficLevel(const std::string& value);
    static const char* trafficName(TrafficLevel level);

private:
    DynamicArray<Location> locations_;
    EdgeNode** adjacency_;
    int adjacencySize_;
    int roadCount_;

    bool addUndirectedRoad(
        const std::string& roadId,
        const std::string& roadName,
        int from,
        int to,
        double distanceKm,
        int baseTimeMinutes,
        TrafficLevel traffic,
        bool blocked
    );

    void addDirectedEdge(
        const std::string& roadId,
        const std::string& roadName,
        int from,
        int to,
        double distanceKm,
        int baseTimeMinutes,
        TrafficLevel traffic,
        bool blocked
    );
};
