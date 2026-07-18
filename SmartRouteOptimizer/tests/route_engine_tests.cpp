#include "DynamicArray.h"
#include "Queue.h"
#include "RouteEngine.h"
#include "Stack.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
bool approximately(double left, double right) {
    return std::fabs(left - right) < 0.001;
}

void testDynamicArray() {
    DynamicArray<int> values(2);
    for (int value = 0; value < 20; ++value) {
        values.pushBack(value * 3);
    }
    assert(values.size() == 20);
    assert(values[0] == 0);
    assert(values[19] == 57);

    DynamicArray<int> copy = values;
    copy[0] = 99;
    assert(values[0] == 0);
    assert(copy[0] == 99);

    int removed = -1;
    assert(copy.popBack(removed));
    assert(removed == 57);
    assert(copy.size() == 19);
}

void testQueue() {
    Queue<int> queue(2);
    for (int value = 1; value <= 12; ++value) {
        queue.enqueue(value);
    }
    assert(queue.size() == 12);

    for (int expected = 1; expected <= 12; ++expected) {
        int actual = 0;
        assert(queue.dequeue(actual));
        assert(actual == expected);
    }
    assert(queue.empty());
}

void testStack() {
    Stack<int> stack;
    stack.push(10);
    stack.push(20);
    stack.push(30);

    int value = 0;
    assert(stack.pop(value) && value == 30);
    assert(stack.pop(value) && value == 20);
    assert(stack.pop(value) && value == 10);
    assert(stack.empty());
}

void testGraphAndRoutes() {
    RouteEngine engine;
    assert(engine.load("data/locations.csv", "data/roads.csv"));
    assert(engine.graph().vertexCount() == 15);
    assert(engine.graph().roadCount() == 32);
    assert(engine.graph().blockedRoadCount() == 0);

    const RouteResult minimumStops = engine.minimumStops(5, 7);
    assert(minimumStops.found);
    assert(minimumStops.stops == 1);
    assert(approximately(minimumStops.distanceKm, 18.0));
    assert(minimumStops.path.size() == 2);
    assert(minimumStops.path[0] == 5 && minimumStops.path[1] == 7);

    const RouteResult minimumDistance = engine.minimumDistance(5, 7);
    assert(minimumDistance.found);
    assert(minimumDistance.stops == 2);
    assert(approximately(minimumDistance.distanceKm, 10.0));
    assert(minimumDistance.path.size() == 3);
    assert(minimumDistance.path[0] == 5);
    assert(minimumDistance.path[1] == 3);
    assert(minimumDistance.path[2] == 7);
    assert(minimumDistance.completedRoutes > 0);

    assert(engine.graph().setRoadBlocked("R22", true));
    assert(engine.graph().blockedRoadCount() == 1);
    const RouteResult reroutedStops = engine.minimumStops(5, 7);
    assert(reroutedStops.found);
    assert(reroutedStops.stops == 2);
    assert(approximately(reroutedStops.distanceKm, 10.0));
    assert(engine.graph().setRoadBlocked("R22", false));

    const RouteResult sameLocation = engine.minimumDistance(4, 4);
    assert(sameLocation.found);
    assert(sameLocation.stops == 0);
    assert(approximately(sameLocation.distanceKm, 0.0));
    assert(sameLocation.path.size() == 1);

    const RouteResult invalid = engine.minimumStops(-1, 99);
    assert(!invalid.found);

    assert(engine.graph().addRoad("R99", 5, 7, 2.0, 3, TrafficLevel::Low));
    assert(engine.graph().roadCount() == 33);
    assert(!engine.graph().addRoad("R99", 1, 2, 4.0, 6, TrafficLevel::Medium));
    const RouteResult expandedNetworkRoute = engine.minimumDistance(5, 7);
    assert(expandedNetworkRoute.found);
    assert(expandedNetworkRoute.stops == 1);
    assert(approximately(expandedNetworkRoute.distanceKm, 2.0));
}
}

int main() {
    testDynamicArray();
    testQueue();
    testStack();
    testGraphAndRoutes();

    std::cout << "DynamicArray tests: passed\n";
    std::cout << "Queue tests: passed\n";
    std::cout << "Stack tests: passed\n";
    std::cout << "Graph loading tests: passed\n";
    std::cout << "BFS Minimum Stops tests: passed\n";
    std::cout << "Recursive DFS Minimum Distance tests: passed\n";
    std::cout << "Blocked-road rerouting tests: passed\n";
    std::cout << "Admin road expansion tests: passed\n";
    return 0;
}
