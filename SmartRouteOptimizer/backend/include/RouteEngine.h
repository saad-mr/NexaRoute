#pragma once

#include "DynamicArray.h"
#include "Graph.h"

#include <filesystem>
#include <string>

struct RouteResult {
    bool found = false;
    std::string algorithm;
    DynamicArray<int> path;
    double distanceKm = 0.0;
    double estimatedMinutes = 0.0;
    int stops = 0;
    int visitedNodes = 0;
    int completedRoutes = 0;
};

class RouteEngine {
public:
    bool load(const std::filesystem::path& locationsFile, const std::filesystem::path& roadsFile);

    RouteResult minimumStops(int start, int destination) const;
    RouteResult minimumDistance(int start, int destination) const;

    const Graph& graph() const;
    Graph& graph();

private:
    Graph graph_;

    void findMinimumDistanceRecursive(
        int current,
        int destination,
        bool* visited,
        DynamicArray<int>& currentPath,
        double currentDistance,
        double& bestDistance,
        DynamicArray<int>& bestPath,
        int& visitedNodes,
        int& completedRoutes
    ) const;

    void calculateMetrics(RouteResult& result) const;
};
