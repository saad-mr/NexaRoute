#include "RouteEngine.h"

#include "Queue.h"
#include "Stack.h"

#include <cmath>
#include <limits>

bool RouteEngine::load(
    const std::filesystem::path& locationsFile,
    const std::filesystem::path& roadsFile
) {
    return graph_.loadFromCsv(locationsFile, roadsFile);
}

RouteResult RouteEngine::minimumStops(int start, int destination) const {
    RouteResult result;
    result.algorithm = "BFS - Minimum Stops";

    const int count = graph_.vertexCount();
    if (!graph_.validVertex(start) || !graph_.validVertex(destination)) {
        return result;
    }

    bool* visited = new bool[count];
    int* previous = new int[count];
    for (int index = 0; index < count; ++index) {
        visited[index] = false;
        previous[index] = -1;
    }

    Queue<int> pending(static_cast<std::size_t>(count));
    visited[start] = true;
    result.visitedNodes = 1;
    pending.enqueue(start);

    int current = -1;
    while (pending.dequeue(current)) {
        if (current == destination) {
            break;
        }

        for (const EdgeNode* edge = graph_.edgesFrom(current); edge != nullptr; edge = edge->next) {
            if (edge->blocked || visited[edge->destination]) {
                continue;
            }
            visited[edge->destination] = true;
            previous[edge->destination] = current;
            ++result.visitedNodes;
            pending.enqueue(edge->destination);
        }
    }

    if (visited[destination]) {
        Stack<int> reversedPath;
        int pathVertex = destination;
        while (pathVertex != -1) {
            reversedPath.push(pathVertex);
            pathVertex = previous[pathVertex];
        }

        int orderedVertex = -1;
        while (reversedPath.pop(orderedVertex)) {
            result.path.pushBack(orderedVertex);
        }

        result.found = true;
        calculateMetrics(result);
    }

    delete[] previous;
    delete[] visited;
    return result;
}

RouteResult RouteEngine::minimumDistance(int start, int destination) const {
    RouteResult result;
    result.algorithm = "Recursive DFS - Minimum Distance";

    const int count = graph_.vertexCount();
    if (!graph_.validVertex(start) || !graph_.validVertex(destination)) {
        return result;
    }

    bool* visited = new bool[count];
    for (int index = 0; index < count; ++index) {
        visited[index] = false;
    }

    DynamicArray<int> currentPath(static_cast<std::size_t>(count));
    DynamicArray<int> bestPath(static_cast<std::size_t>(count));
    double bestDistance = std::numeric_limits<double>::infinity();

    visited[start] = true;
    currentPath.pushBack(start);

    findMinimumDistanceRecursive(
        start,
        destination,
        visited,
        currentPath,
        0.0,
        bestDistance,
        bestPath,
        result.visitedNodes,
        result.completedRoutes
    );

    delete[] visited;

    if (!bestPath.empty()) {
        result.found = true;
        result.path = bestPath;
        calculateMetrics(result);
    }
    return result;
}

const Graph& RouteEngine::graph() const {
    return graph_;
}

Graph& RouteEngine::graph() {
    return graph_;
}

void RouteEngine::findMinimumDistanceRecursive(
    int current,
    int destination,
    bool* visited,
    DynamicArray<int>& currentPath,
    double currentDistance,
    double& bestDistance,
    DynamicArray<int>& bestPath,
    int& visitedNodes,
    int& completedRoutes
) const {
    ++visitedNodes;

    if (current == destination) {
        ++completedRoutes;
        if (currentDistance < bestDistance) {
            bestDistance = currentDistance;
            bestPath = currentPath;
        }
        return;
    }

    for (const EdgeNode* edge = graph_.edgesFrom(current); edge != nullptr; edge = edge->next) {
        if (edge->blocked || visited[edge->destination]) {
            continue;
        }

        const double nextDistance = currentDistance + edge->distanceKm;
        if (nextDistance >= bestDistance) {
            continue;
        }

        visited[edge->destination] = true;
        currentPath.pushBack(edge->destination);

        findMinimumDistanceRecursive(
            edge->destination,
            destination,
            visited,
            currentPath,
            nextDistance,
            bestDistance,
            bestPath,
            visitedNodes,
            completedRoutes
        );

        int removedVertex = -1;
        currentPath.popBack(removedVertex);
        visited[edge->destination] = false;
    }
}

void RouteEngine::calculateMetrics(RouteResult& result) const {
    result.distanceKm = 0.0;
    result.estimatedMinutes = 0.0;

    if (result.path.size() < 2) {
        result.stops = 0;
        return;
    }

    result.stops = static_cast<int>(result.path.size()) - 1;
    for (std::size_t index = 0; index + 1 < result.path.size(); ++index) {
        const EdgeNode* edge = graph_.edgeBetween(result.path[index], result.path[index + 1]);
        if (edge == nullptr) {
            result.found = false;
            return;
        }
        result.distanceKm += edge->distanceKm;
        result.estimatedMinutes += edge->baseTimeMinutes * Graph::trafficMultiplier(edge->traffic);
    }

    result.distanceKm = std::round(result.distanceKm * 10.0) / 10.0;
    result.estimatedMinutes = std::round(result.estimatedMinutes * 10.0) / 10.0;
}
