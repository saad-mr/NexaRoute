#include "Graph.h"

#include "Validation.h"

#include <fstream>
#include <sstream>

namespace {
struct CsvRow {
    std::string values[16];
    int count = 0;

    const std::string& operator[](int index) const {
        return values[index];
    }
};

CsvRow splitCsvLine(const std::string& line) {
    CsvRow row;
    std::stringstream stream(line);
    std::string value;
    while (row.count < 16 && std::getline(stream, value, ',')) {
        row.values[row.count++] = value;
    }
    return row;
}
}

Graph::Graph()
    : locations_(), adjacency_(nullptr), adjacencySize_(0), roadCount_(0) {
}

Graph::~Graph() {
    clear();
}

bool Graph::loadFromCsv(
    const std::filesystem::path& locationsFile,
    const std::filesystem::path& roadsFile
) {
    clear();

    std::ifstream locationInput(locationsFile);
    std::ifstream roadInput(roadsFile);
    if (!locationInput || !roadInput) {
        return false;
    }

    std::string line;
    std::getline(locationInput, line);
    while (std::getline(locationInput, line)) {
        const CsvRow fields = splitCsvLine(line);
        if (fields.count != 5) {
            clear();
            return false;
        }

        try {
            Location value;
            value.id = std::stoi(fields[0]);
            value.name = fields[1];
            value.type = fields[2];
            value.x = std::stoi(fields[3]);
            value.y = std::stoi(fields[4]);
            locations_.pushBack(value);
        } catch (...) {
            clear();
            return false;
        }
    }

    if (locations_.empty()) {
        return false;
    }

    adjacencySize_ = static_cast<int>(locations_.size());
    adjacency_ = new EdgeNode*[adjacencySize_];
    for (int index = 0; index < adjacencySize_; ++index) {
        adjacency_[index] = nullptr;
        if (locations_[static_cast<std::size_t>(index)].id != index) {
            clear();
            return false;
        }
    }

    std::getline(roadInput, line);
    while (std::getline(roadInput, line)) {
        const CsvRow fields = splitCsvLine(line);
        if (fields.count != 7) {
            clear();
            return false;
        }

        try {
            const int from = std::stoi(fields[1]);
            const int to = std::stoi(fields[2]);
            const double distance = std::stod(fields[3]);
            const int baseTime = std::stoi(fields[4]);
            const bool blocked = fields[6] == "1";

            if (!addUndirectedRoad(
                    fields[0],
                    locations_[static_cast<std::size_t>(from)].name + " to " +
                        locations_[static_cast<std::size_t>(to)].name,
                    from, to, distance, baseTime,
                    parseTrafficLevel(fields[5]), blocked)) {
                clear();
                return false;
            }
        } catch (...) {
            clear();
            return false;
        }
    }

    return roadCount_ > 0;
}

void Graph::clear() {
    if (adjacency_ != nullptr) {
        for (int index = 0; index < adjacencySize_; ++index) {
            EdgeNode* current = adjacency_[index];
            while (current != nullptr) {
                EdgeNode* removed = current;
                current = current->next;
                delete removed;
            }
        }
        delete[] adjacency_;
    }

    adjacency_ = nullptr;
    adjacencySize_ = 0;
    roadCount_ = 0;
    locations_.clear();
}

int Graph::vertexCount() const {
    return adjacencySize_;
}

int Graph::roadCount() const {
    return roadCount_;
}

int Graph::blockedRoadCount() const {
    int blockedEdges = 0;
    for (int index = 0; index < adjacencySize_; ++index) {
        for (const EdgeNode* edge = adjacency_[index]; edge != nullptr; edge = edge->next) {
            if (edge->blocked) {
                ++blockedEdges;
            }
        }
    }
    return blockedEdges / 2;
}

bool Graph::validVertex(int id) const {
    return id >= 0 && id < adjacencySize_;
}

const Location* Graph::location(int id) const {
    if (!validVertex(id)) {
        return nullptr;
    }
    return &locations_[static_cast<std::size_t>(id)];
}

const Location* Graph::locationByName(const std::string& name) const {
    for (std::size_t index = 0; index < locations_.size(); ++index) {
        if (Validation::equalsIgnoreCase(locations_[index].name, name)) {
            return &locations_[index];
        }
    }
    return nullptr;
}

const EdgeNode* Graph::edgesFrom(int id) const {
    if (!validVertex(id)) {
        return nullptr;
    }
    return adjacency_[id];
}

const EdgeNode* Graph::edgeBetween(int from, int to) const {
    if (!validVertex(from) || !validVertex(to)) {
        return nullptr;
    }

    for (const EdgeNode* edge = adjacency_[from]; edge != nullptr; edge = edge->next) {
        if (edge->destination == to) {
            return edge;
        }
    }
    return nullptr;
}

const EdgeNode* Graph::roadById(const std::string& roadId) const {
    for (int index = 0; index < adjacencySize_; ++index) {
        for (const EdgeNode* edge = adjacency_[index]; edge != nullptr; edge = edge->next) {
            if (edge->roadId == roadId) {
                return edge;
            }
        }
    }
    return nullptr;
}

bool Graph::setRoadBlocked(const std::string& roadId, bool blocked) {
    bool changed = false;
    for (int index = 0; index < adjacencySize_; ++index) {
        for (EdgeNode* edge = adjacency_[index]; edge != nullptr; edge = edge->next) {
            if (edge->roadId == roadId) {
                edge->blocked = blocked;
                changed = true;
            }
        }
    }
    return changed;
}

bool Graph::setRoadTraffic(const std::string& roadId, TrafficLevel level) {
    bool changed = false;
    for (int index = 0; index < adjacencySize_; ++index) {
        for (EdgeNode* edge = adjacency_[index]; edge != nullptr; edge = edge->next) {
            if (edge->roadId == roadId) {
                edge->traffic = level;
                changed = true;
            }
        }
    }
    return changed;
}

int Graph::addLocation(
    const std::string& name,
    const std::string& type,
    int x,
    int y
) {
    const std::string cleanName = Validation::collapseSpaces(Validation::trim(name));
    const std::string cleanType = Validation::collapseSpaces(Validation::trim(type));
    if (!Validation::isLocationName(cleanName) ||
        !Validation::isLocationType(cleanType) ||
        !Validation::isMapCoordinate(x, y) ||
        locationByName(cleanName) != nullptr) {
        return -1;
    }

    EdgeNode** expanded = new EdgeNode*[adjacencySize_ + 1];
    for (int index = 0; index < adjacencySize_; ++index) {
        expanded[index] = adjacency_[index];
    }
    expanded[adjacencySize_] = nullptr;
    delete[] adjacency_;
    adjacency_ = expanded;

    Location locationValue;
    locationValue.id = adjacencySize_;
    locationValue.name = cleanName;
    locationValue.type = cleanType;
    locationValue.x = x;
    locationValue.y = y;
    locations_.pushBack(std::move(locationValue));
    ++adjacencySize_;
    return adjacencySize_ - 1;
}

bool Graph::addRoad(
    const std::string& roadId,
    int from,
    int to,
    double distanceKm,
    int baseTimeMinutes,
    TrafficLevel traffic
) {
    return addRoad(
        roadId,
        roadId,
        from,
        to,
        distanceKm,
        baseTimeMinutes,
        traffic
    );
}

bool Graph::addRoad(
    const std::string& roadId,
    const std::string& roadName,
    int from,
    int to,
    double distanceKm,
    int baseTimeMinutes,
    TrafficLevel traffic
) {
    if (roadId.empty() || roadById(roadId) != nullptr) return false;
    return addUndirectedRoad(
        roadId,
        Validation::collapseSpaces(Validation::trim(roadName)),
        from,
        to,
        distanceKm,
        baseTimeMinutes,
        traffic,
        false
    );
}

double Graph::trafficMultiplier(TrafficLevel level) {
    switch (level) {
        case TrafficLevel::Medium: return 1.25;
        case TrafficLevel::High: return 1.60;
        default: return 1.00;
    }
}

TrafficLevel Graph::parseTrafficLevel(const std::string& value) {
    if (value == "High") return TrafficLevel::High;
    if (value == "Medium") return TrafficLevel::Medium;
    return TrafficLevel::Low;
}

const char* Graph::trafficName(TrafficLevel level) {
    switch (level) {
        case TrafficLevel::Medium: return "Medium";
        case TrafficLevel::High: return "High";
        default: return "Low";
    }
}

bool Graph::addUndirectedRoad(
    const std::string& roadId,
    const std::string& roadName,
    int from,
    int to,
    double distanceKm,
    int baseTimeMinutes,
    TrafficLevel traffic,
    bool blocked
) {
    if (!validVertex(from) || !validVertex(to) || from == to ||
        !Validation::isIdentifier(roadId, 2, 20) ||
        !Validation::isRouteName(roadName) ||
        !Validation::isDistance(distanceKm) ||
        !Validation::isTravelTime(baseTimeMinutes)) {
        return false;
    }

    addDirectedEdge(roadId, roadName, from, to, distanceKm, baseTimeMinutes, traffic, blocked);
    addDirectedEdge(roadId, roadName, to, from, distanceKm, baseTimeMinutes, traffic, blocked);
    ++roadCount_;
    return true;
}

void Graph::addDirectedEdge(
    const std::string& roadId,
    const std::string& roadName,
    int from,
    int to,
    double distanceKm,
    int baseTimeMinutes,
    TrafficLevel traffic,
    bool blocked
) {
    EdgeNode* edge = new EdgeNode;
    edge->roadId = roadId;
    edge->roadName = roadName;
    edge->destination = to;
    edge->distanceKm = distanceKm;
    edge->baseTimeMinutes = baseTimeMinutes;
    edge->traffic = traffic;
    edge->blocked = blocked;
    edge->next = adjacency_[from];
    adjacency_[from] = edge;
}
