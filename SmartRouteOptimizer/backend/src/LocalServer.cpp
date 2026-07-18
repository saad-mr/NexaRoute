#include "LocalServer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
#endif

NativeSocket toNative(std::intptr_t value) {
    return static_cast<NativeSocket>(value);
}

std::intptr_t fromNative(NativeSocket value) {
    return static_cast<std::intptr_t>(value);
}

void closeNativeSocket(NativeSocket socketValue) {
    if (socketValue == INVALID_NATIVE_SOCKET) {
        return;
    }
#ifdef _WIN32
    closesocket(socketValue);
#else
    close(socketValue);
#endif
}

std::string statusText(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Internal Server Error";
    }
}

int hexadecimalValue(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded += ' ';
        } else if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hexadecimalValue(value[index + 1]);
            const int low = hexadecimalValue(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>(high * 16 + low);
                index += 2;
            } else {
                decoded += value[index];
            }
        } else {
            decoded += value[index];
        }
    }
    return decoded;
}

bool encodedValue(const std::string& encoded, const std::string& key, std::string& value) {
    const std::string token = key + "=";
    std::size_t position = 0;
    while (position <= encoded.size()) {
        const std::size_t end = encoded.find('&', position);
        const std::string pair = encoded.substr(
            position,
            end == std::string::npos ? std::string::npos : end - position
        );
        if (pair.rfind(token, 0) == 0) {
            value = urlDecode(pair.substr(token.size()));
            return true;
        }
        if (end == std::string::npos) break;
        position = end + 1;
    }
    return false;
}

bool queryText(const std::string& target, const std::string& key, std::string& value) {
    const std::size_t queryStart = target.find('?');
    if (queryStart == std::string::npos) return false;
    return encodedValue(target.substr(queryStart + 1), key, value);
}

std::size_t contentLengthFromHeaders(const std::string& headers) {
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    const std::string name = "content-length:";
    const std::size_t position = lower.find(name);
    if (position == std::string::npos) return 0;

    std::size_t valueStart = position + name.size();
    while (valueStart < lower.size() && std::isspace(static_cast<unsigned char>(lower[valueStart]))) {
        ++valueStart;
    }
    const std::size_t valueEnd = lower.find("\r\n", valueStart);
    try {
        return static_cast<std::size_t>(std::stoul(lower.substr(valueStart, valueEnd - valueStart)));
    } catch (...) {
        return 0;
    }
}

std::string escapeJsonValue(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}

void appendRouteJson(std::ostringstream& json, const RouteResult& result, const Graph& graph) {
    json << "{\"found\":" << (result.found ? "true" : "false")
         << ",\"algorithm\":\"" << result.algorithm << "\""
         << ",\"distance\":" << std::fixed << std::setprecision(1) << result.distanceKm
         << ",\"estimatedMinutes\":" << result.estimatedMinutes
         << ",\"stops\":" << result.stops
         << ",\"visitedNodes\":" << result.visitedNodes
         << ",\"completedRoutes\":" << result.completedRoutes
         << ",\"path\":[";

    for (std::size_t index = 0; index < result.path.size(); ++index) {
        if (index > 0) {
            json << ',';
        }
        const int locationId = result.path[index];
        const Location* location = graph.location(locationId);
        json << "{\"id\":" << locationId << ",\"name\":\"";
        if (location != nullptr) {
            json << escapeJsonValue(location->name);
        }
        json << "\"}";
    }
    json << "]}";
}
}

LocalServer::LocalServer(
    std::filesystem::path projectRoot,
    unsigned short port,
    bool persistenceEnabled,
    bool automationEnabled
)
    : projectRoot_(std::move(projectRoot)),
      runtimeDirectory_(projectRoot_ / "data" / "runtime"),
      port_(port),
      persistenceEnabled_(persistenceEnabled),
      automationEnabled_(automationEnabled),
      running_(false),
      listenSocket_(-1),
      routeEngine_(),
      networkManager_(),
      routeEngineReady_(false),
      deliverySystem_(),
      authService_(),
      analyticsService_(),
      roadChangeHistory_() {
    routeEngineReady_ = routeEngine_.load(
        projectRoot_ / "data" / "locations.csv",
        projectRoot_ / "data" / "roads.csv"
    );
    if (routeEngineReady_) {
        const bool networkReady = networkManager_.initialize(
            &routeEngine_,
            runtimeDirectory_,
            persistenceEnabled_
        );
        const bool deliveryReady = deliverySystem_.initialize(
            &routeEngine_,
            projectRoot_ / "data" / "riders.csv",
            persistenceEnabled_ ? runtimeDirectory_ : std::filesystem::path{}
        );
        const bool authenticationReady = deliveryReady && authService_.initialize(
            runtimeDirectory_,
            persistenceEnabled_
        );
        const bool analyticsReady = deliveryReady && analyticsService_.initialize(
            &deliverySystem_,
            &routeEngine_.graph()
        );
        routeEngineReady_ = networkReady && deliveryReady &&
                            authenticationReady && analyticsReady;
    }
}

LocalServer::~LocalServer() {
    stop();
    cleanupNetwork();
}

bool LocalServer::initializeNetwork() {
#ifdef _WIN32
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void LocalServer::cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool LocalServer::start() {
    if (!initializeNetwork()) {
        std::cerr << "Network initialization failed.\n";
        return false;
    }

    const NativeSocket serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_NATIVE_SOCKET) {
        std::cerr << "Could not create the local server socket.\n";
        return false;
    }

    listenSocket_ = fromNative(serverSocket);

    int reuseAddress = 1;
#ifdef _WIN32
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        std::cerr << "Port " << port_ << " is unavailable. Close the other program or choose another port.\n";
        closeNativeSocket(serverSocket);
        listenSocket_ = -1;
        return false;
    }

    if (listen(serverSocket, 16) != 0) {
        std::cerr << "The local server could not start listening.\n";
        closeNativeSocket(serverSocket);
        listenSocket_ = -1;
        return false;
    }

    running_ = true;

    while (running_) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientLength = sizeof(clientAddress);
#else
        socklen_t clientLength = sizeof(clientAddress);
#endif
        const NativeSocket clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientSocket == INVALID_NATIVE_SOCKET) {
            if (running_) {
                std::cerr << "A local connection could not be accepted.\n";
            }
            continue;
        }

        handleClient(fromNative(clientSocket));
        closeNativeSocket(clientSocket);
    }

    return true;
}

void LocalServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    const NativeSocket serverSocket = toNative(listenSocket_);
    listenSocket_ = -1;

#ifdef _WIN32
    shutdown(serverSocket, SD_BOTH);
#else
    shutdown(serverSocket, SHUT_RDWR);
#endif
    closeNativeSocket(serverSocket);
}

unsigned short LocalServer::port() const {
    return port_;
}

const std::filesystem::path& LocalServer::projectRoot() const {
    return projectRoot_;
}

void LocalServer::handleClient(std::intptr_t clientSocket) {
    const HttpRequest request = readRequest(clientSocket);
    int statusCode = 200;
    std::string contentType = "application/json; charset=utf-8";
    const std::string body = routeRequest(request, statusCode, contentType);
    const std::string response = makeHttpResponse(statusCode, contentType, body);

    const NativeSocket socketValue = toNative(clientSocket);
    std::size_t sentTotal = 0;
    while (sentTotal < response.size()) {
        const int sent = send(
            socketValue,
            response.data() + sentTotal,
            static_cast<int>(response.size() - sentTotal),
            0
        );
        if (sent <= 0) {
            break;
        }
        sentTotal += static_cast<std::size_t>(sent);
    }
}

LocalServer::HttpRequest LocalServer::readRequest(std::intptr_t clientSocket) const {
    HttpRequest request;
    std::string rawRequest;
    char buffer[4096];
    const NativeSocket socketValue = toNative(clientSocket);

    std::size_t headersEnd = std::string::npos;
    std::size_t expectedBodyLength = 0;

    while (rawRequest.size() < 1024 * 1024) {
        const int received = recv(socketValue, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (received <= 0) {
            break;
        }
        rawRequest.append(buffer, static_cast<std::size_t>(received));

        if (headersEnd == std::string::npos) {
            headersEnd = rawRequest.find("\r\n\r\n");
            if (headersEnd != std::string::npos) {
                expectedBodyLength = contentLengthFromHeaders(rawRequest.substr(0, headersEnd));
            }
        }

        if (headersEnd != std::string::npos &&
            rawRequest.size() >= headersEnd + 4 + expectedBodyLength) {
            break;
        }
    }

    const std::size_t firstLineEnd = rawRequest.find("\r\n");
    if (firstLineEnd == std::string::npos) {
        return request;
    }

    std::istringstream firstLine(rawRequest.substr(0, firstLineEnd));
    firstLine >> request.method >> request.target;

    if (headersEnd != std::string::npos) {
        request.body = rawRequest.substr(headersEnd + 4, expectedBodyLength);
    }

    return request;
}

std::string LocalServer::routeRequest(
    const HttpRequest& request,
    int& statusCode,
    std::string& contentType
) {
    if (request.method.empty() || request.target.empty()) {
        statusCode = 400;
        return R"({"error":"Malformed request"})";
    }

    if (request.method != "GET" && request.method != "POST") {
        statusCode = 405;
        return R"({"error":"Only GET and POST requests are supported"})";
    }

    if (automationEnabled_ && deliverySystem_.ready()) {
        deliverySystem_.advanceDueBookings();
    }

    std::string path = request.target;
    const std::size_t queryPosition = path.find('?');
    if (queryPosition != std::string::npos) {
        path = path.substr(0, queryPosition);
    }

    if (path == "/api/health") {
        contentType = "application/json; charset=utf-8";
        if (!routeEngineReady_ || !deliverySystem_.ready()) {
            statusCode = 500;
            return R"({"status":"error","service":"NexaRoute C++ Engine","version":"3.1.0","city":"Nexa City","connection":"Core data failed to load"})";
        }
        return R"({"status":"ok","service":"NexaRoute C++ Engine","version":"3.1.0","city":"Nexa City","connection":"C++ route, delivery, network, and authentication services connected"})";
    }

    if (request.method == "GET" && path == "/api/city") {
        contentType = "application/json; charset=utf-8";
        return buildCityJson();
    }

    if (request.method == "POST" && path == "/api/auth/rider/login") {
        contentType = "application/json; charset=utf-8";
        std::string riderId;
        std::string password;
        if (!encodedValue(request.body, "riderId", riderId) ||
            !encodedValue(request.body, "password", password)) {
            statusCode = 400;
            return R"({"error":"Rider ID and password are required"})";
        }
        const AuthResult result = authService_.loginRider(riderId, password);
        if (!result.success) {
            statusCode = 401;
            return "{\"error\":\"" + jsonEscape(result.error) + "\"}";
        }
        const Rider* rider = deliverySystem_.findRider(result.accountId);
        return "{\"session\":" + buildSessionJson(result) +
               ",\"rider\":" + (rider == nullptr ? "null" : buildRiderJson(*rider, true)) + "}";
    }

    if (request.method == "POST" && path == "/api/auth/admin/login") {
        contentType = "application/json; charset=utf-8";
        std::string username;
        std::string password;
        if (!encodedValue(request.body, "username", username) ||
            !encodedValue(request.body, "password", password)) {
            statusCode = 400;
            return R"({"error":"Admin username and password are required"})";
        }
        const AuthResult result = authService_.loginAdmin(username, password);
        if (!result.success) {
            statusCode = 401;
            return "{\"error\":\"" + jsonEscape(result.error) + "\"}";
        }
        return "{\"session\":" + buildSessionJson(result) + "}";
    }

    if (request.method == "GET" && path == "/api/auth/session") {
        contentType = "application/json; charset=utf-8";
        const std::string token = requestToken(request);
        const AuthSession* activeSession = authService_.session(token);
        if (activeSession == nullptr) {
            statusCode = 401;
            return R"({"error":"Your session is missing or has expired"})";
        }
        AuthResult result;
        result.success = true;
        result.token = activeSession->token;
        result.accountId = activeSession->accountId;
        result.role = activeSession->role;
        result.expiresAtEpoch = activeSession->expiresAtEpoch;
        std::string response = "{\"session\":" + buildSessionJson(result);
        if (activeSession->role == AccountRole::Rider) {
            const Rider* rider = deliverySystem_.findRider(activeSession->accountId);
            response += ",\"rider\":" + (rider == nullptr ? "null" : buildRiderJson(*rider, true));
        }
        return response + "}";
    }

    if (request.method == "POST" && path == "/api/auth/logout") {
        contentType = "application/json; charset=utf-8";
        if (!authService_.logout(requestToken(request))) {
            statusCode = 401;
            return R"({"error":"No active session was found"})";
        }
        return R"({"success":true})";
    }

    if (request.method == "POST" && path == "/api/auth/change-password") {
        contentType = "application/json; charset=utf-8";
        std::string currentPassword;
        std::string newPassword;
        if (!encodedValue(request.body, "currentPassword", currentPassword) ||
            !encodedValue(request.body, "newPassword", newPassword)) {
            statusCode = 400;
            return R"({"error":"Current and new passwords are required"})";
        }
        std::string error;
        if (!authService_.changePassword(requestToken(request), currentPassword, newPassword, error)) {
            statusCode = error.find("session") != std::string::npos ? 401 : 400;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        return R"({"success":true})";
    }

    if (request.method == "GET" && path == "/api/riders") {
        contentType = "application/json; charset=utf-8";
        return buildRidersJson(false);
    }

    if (request.method == "GET" && path == "/api/rider/profile") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Rider, statusCode, authorizationError)) {
            return authorizationError;
        }
        const AuthSession* activeSession = authService_.session(requestToken(request));
        const Rider* rider = activeSession == nullptr
            ? nullptr
            : deliverySystem_.findRider(activeSession->accountId);
        if (rider == nullptr) {
            statusCode = 404;
            return R"({"error":"Rider profile was not found"})";
        }
        return "{\"rider\":" + buildRiderJson(*rider, true) + "}";
    }

    if (request.method == "POST" && path == "/api/riders/register") {
        contentType = "application/json; charset=utf-8";
        std::string name;
        std::string phone;
        std::string location;
        std::string vehicle;
        std::string registration;
        std::string baseCharge;
        std::string perKmCharge;
        std::string password;
        if (!encodedValue(request.body, "name", name) ||
            !encodedValue(request.body, "phone", phone) ||
            !encodedValue(request.body, "locationId", location) ||
            !encodedValue(request.body, "vehicle", vehicle) ||
            !encodedValue(request.body, "registration", registration) ||
            !encodedValue(request.body, "baseCharge", baseCharge) ||
            !encodedValue(request.body, "perKmCharge", perKmCharge) ||
            !encodedValue(request.body, "password", password)) {
            statusCode = 400;
            return R"({"error":"Complete rider registration details and a password are required"})";
        }
        const std::string passwordError = AuthService::passwordError(password);
        if (!passwordError.empty()) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(passwordError) + "\"}";
        }

        RegisterRiderRequest riderRequest;
        riderRequest.name = name;
        riderRequest.phone = phone;
        riderRequest.vehicle = vehicle;
        riderRequest.registration = registration;
        try {
            riderRequest.locationId = std::stoi(location);
            riderRequest.baseCharge = std::stod(baseCharge);
            riderRequest.perKmCharge = std::stod(perKmCharge);
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Rider location and charges must be valid numbers"})";
        }

        std::string error;
        Rider* rider = deliverySystem_.registerRider(riderRequest, error);
        if (rider == nullptr) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        if (!authService_.registerRiderCredential(rider->id, password, error)) {
            statusCode = 500;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        const AuthResult login = authService_.loginRider(rider->id, password);
        if (!login.success) {
            statusCode = 500;
            return R"({"error":"Rider profile was created, but its session could not be started"})";
        }
        statusCode = 201;
        return "{\"session\":" + buildSessionJson(login) +
               ",\"rider\":" + buildRiderJson(*rider, true) + "}";
    }

    if (request.method == "POST" && path == "/api/riders/update") {
        contentType = "application/json; charset=utf-8";
        std::string riderId;
        std::string available;
        std::string location;
        std::string baseCharge;
        std::string perKmCharge;
        if (!encodedValue(request.body, "riderId", riderId) ||
            !encodedValue(request.body, "available", available) ||
            !encodedValue(request.body, "locationId", location) ||
            !encodedValue(request.body, "baseCharge", baseCharge) ||
            !encodedValue(request.body, "perKmCharge", perKmCharge)) {
            statusCode = 400;
            return R"({"error":"Rider ID, availability, current location, and charges are required"})";
        }
        std::string authorizationError;
        if (!authorize(request, AccountRole::Rider, statusCode, authorizationError, riderId)) {
            return authorizationError;
        }
        std::string error;
        try {
            if (!deliverySystem_.updateRider(
                    riderId,
                    available == "1" || available == "true",
                    std::stoi(location),
                    std::stod(baseCharge),
                    std::stod(perKmCharge),
                    error)) {
                statusCode = 400;
                return "{\"error\":\"" + jsonEscape(error) + "\"}";
            }
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Rider location and charges must be valid numbers"})";
        }
        const Rider* rider = deliverySystem_.findRider(riderId);
        return rider == nullptr
            ? R"({"error":"Rider ID was not found"})"
            : "{\"rider\":" + buildRiderJson(*rider, true) + "}";
    }

    if (request.method == "GET" && path == "/api/route") {
        contentType = "application/json; charset=utf-8";
        if (!routeEngineReady_) {
            statusCode = 500;
            return R"({"error":"The route graph is unavailable"})";
        }

        std::string fromValue;
        std::string toValue;
        if (!queryText(request.target, "from", fromValue) ||
            !queryText(request.target, "to", toValue)) {
            statusCode = 400;
            return R"({"error":"Pickup and destination names are required"})";
        }
        const int from = networkManager_.resolveLocationId(fromValue);
        const int to = networkManager_.resolveLocationId(toValue);
        if (from < 0 || to < 0 || from == to) {
            statusCode = 400;
            return R"({"error":"Choose two different saved locations by name or ID"})";
        }
        return buildRouteComparisonJson(from, to);
    }

    if (request.method == "GET" && path == "/api/dashboard") {
        contentType = "application/json; charset=utf-8";
        return buildDashboardJson(false);
    }

    if (request.method == "GET" && path == "/api/bookings") {
        contentType = "application/json; charset=utf-8";
        return buildBookingsJson();
    }

    if (request.method == "GET" && path == "/api/admin/dashboard") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        return buildDashboardJson(true);
    }

    if (request.method == "GET" && path == "/api/admin/analytics") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        return buildAnalyticsJson();
    }

    if (request.method == "GET" && path == "/api/admin/riders") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        return buildRidersJson(true);
    }

    if (request.method == "GET" && path == "/api/admin/bookings") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        return buildBookingsJson();
    }

    if (request.method == "GET" && path == "/api/track") {
        contentType = "application/json; charset=utf-8";
        std::string trackingId;
        if (!queryText(request.target, "id", trackingId)) {
            statusCode = 400;
            return R"({"error":"A tracking ID is required"})";
        }
        const Booking* booking = deliverySystem_.findBooking(trackingId);
        if (booking == nullptr) {
            statusCode = 404;
            return R"({"error":"Tracking ID was not found"})";
        }
        return buildBookingJson(*booking);
    }

    if (request.method == "POST" && path == "/api/bookings") {
        contentType = "application/json; charset=utf-8";
        std::string customer;
        std::string receiver;
        std::string phone;
        std::string pickup;
        std::string destination;
        std::string weight;
        std::string priority;
        std::string mode;

        if (!encodedValue(request.body, "customerName", customer) ||
            !encodedValue(request.body, "receiverName", receiver) ||
            !encodedValue(request.body, "receiverPhone", phone) ||
            !encodedValue(request.body, "pickupId", pickup) ||
            !encodedValue(request.body, "destinationId", destination) ||
            !encodedValue(request.body, "weightKg", weight) ||
            !encodedValue(request.body, "priority", priority) ||
            !encodedValue(request.body, "mode", mode)) {
            statusCode = 400;
            return R"({"error":"Complete booking details are required"})";
        }

        CreateBookingRequest bookingRequest;
        bookingRequest.customerName = customer;
        bookingRequest.receiverName = receiver;
        bookingRequest.receiverPhone = phone;
        bookingRequest.priority = DeliverySystem::parsePriority(priority);
        bookingRequest.mode = DeliverySystem::parseMode(mode);
        try {
            bookingRequest.pickupId = std::stoi(pickup);
            bookingRequest.destinationId = std::stoi(destination);
            bookingRequest.weightKg = std::stod(weight);
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Location IDs and parcel weight must be valid numbers"})";
        }

        BookingCreationResult created = deliverySystem_.createBooking(bookingRequest);
        if (!created.success || created.booking == nullptr) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(created.error) + "\"}";
        }
        statusCode = 201;
        return buildBookingJson(*created.booking);
    }

    if (request.method == "POST" && path == "/api/bookings/accept-offer") {
        contentType = "application/json; charset=utf-8";
        std::string trackingId;
        std::string riderId;
        if (!encodedValue(request.body, "trackingId", trackingId) ||
            !encodedValue(request.body, "riderId", riderId)) {
            statusCode = 400;
            return R"({"error":"Tracking and rider IDs are required"})";
        }
        std::string error;
        if (!deliverySystem_.acceptOffer(trackingId, riderId, error)) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        return buildBookingJson(*deliverySystem_.findBooking(trackingId));
    }

    if (request.method == "POST" && path == "/api/bookings/advance") {
        contentType = "application/json; charset=utf-8";
        std::string trackingId;
        if (!encodedValue(request.body, "trackingId", trackingId)) {
            statusCode = 400;
            return R"({"error":"A tracking ID is required"})";
        }
        std::string error;
        if (!deliverySystem_.advanceBooking(trackingId, error)) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        return buildBookingJson(*deliverySystem_.findBooking(trackingId));
    }

    if (request.method == "POST" && path == "/api/bookings/cancel") {
        contentType = "application/json; charset=utf-8";
        std::string trackingId;
        if (!encodedValue(request.body, "trackingId", trackingId)) {
            statusCode = 400;
            return R"({"error":"A tracking ID is required"})";
        }
        std::string error;
        if (!deliverySystem_.cancelBooking(trackingId, error)) {
            statusCode = 400;
            return "{\"error\":\"" + jsonEscape(error) + "\"}";
        }
        return buildBookingJson(*deliverySystem_.findBooking(trackingId));
    }

    if (request.method == "POST" && path == "/api/bookings/rate") {
        contentType = "application/json; charset=utf-8";
        std::string trackingId;
        std::string ratingValue;
        if (!encodedValue(request.body, "trackingId", trackingId) ||
            !encodedValue(request.body, "rating", ratingValue)) {
            statusCode = 400;
            return R"({"error":"Tracking ID and rating are required"})";
        }
        std::string error;
        try {
            if (!deliverySystem_.rateDelivery(trackingId, std::stoi(ratingValue), error)) {
                statusCode = 400;
                return "{\"error\":\"" + jsonEscape(error) + "\"}";
            }
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Rating must be a whole number from 1 to 5"})";
        }
        return buildBookingJson(*deliverySystem_.findBooking(trackingId));
    }

    if (request.method == "POST" && path == "/api/roads/add") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        std::string routeName;
        std::string fromValue;
        std::string toValue;
        std::string distanceValue;
        std::string timeValue;
        std::string traffic;
        if (!encodedValue(request.body, "routeName", routeName) ||
            !encodedValue(request.body, "from", fromValue) ||
            !encodedValue(request.body, "to", toValue) ||
            !encodedValue(request.body, "distance", distanceValue) ||
            !encodedValue(request.body, "baseTime", timeValue) ||
            !encodedValue(request.body, "traffic", traffic)) {
            statusCode = 400;
            return R"({"error":"A route name and complete road details are required"})";
        }
        try {
            AddExistingRoadRequest roadRequest;
            roadRequest.routeName = routeName;
            roadRequest.fromId = networkManager_.resolveLocationId(fromValue);
            roadRequest.toId = networkManager_.resolveLocationId(toValue);
            roadRequest.distanceKm = std::stod(distanceValue);
            roadRequest.baseTimeMinutes = std::stoi(timeValue);
            roadRequest.traffic = Graph::parseTrafficLevel(traffic);
            const NetworkChangeResult result = networkManager_.addRoad(roadRequest);
            if (!result.success) {
                statusCode = 400;
                return "{\"error\":\"" + jsonEscape(result.error) + "\"}";
            }
            deliverySystem_.recalculateActiveRoutes();
            return buildCityJson();
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Road distance, time, and locations must be valid numbers"})";
        }
    }

    if (request.method == "POST" && path == "/api/locations/add") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        std::string locationName;
        std::string locationType;
        std::string connectFrom;
        std::string routeName;
        std::string distanceValue;
        std::string timeValue;
        std::string traffic;
        if (!encodedValue(request.body, "locationName", locationName) ||
            !encodedValue(request.body, "locationType", locationType) ||
            !encodedValue(request.body, "connectFrom", connectFrom) ||
            !encodedValue(request.body, "routeName", routeName) ||
            !encodedValue(request.body, "distance", distanceValue) ||
            !encodedValue(request.body, "baseTime", timeValue) ||
            !encodedValue(request.body, "traffic", traffic)) {
            statusCode = 400;
            return R"({"error":"Complete destination and connecting route details are required"})";
        }
        try {
            AddNamedDestinationRequest locationRequest;
            locationRequest.locationName = locationName;
            locationRequest.locationType = locationType;
            locationRequest.connectFromId = networkManager_.resolveLocationId(connectFrom);
            locationRequest.routeName = routeName;
            locationRequest.distanceKm = std::stod(distanceValue);
            locationRequest.baseTimeMinutes = std::stoi(timeValue);
            locationRequest.traffic = Graph::parseTrafficLevel(traffic);
            const NetworkChangeResult result = networkManager_.addNamedDestination(locationRequest);
            if (!result.success) {
                statusCode = 400;
                return "{\"error\":\"" + jsonEscape(result.error) + "\"}";
            }
            deliverySystem_.recalculateActiveRoutes();
            statusCode = 201;
            return buildCityJson();
        } catch (...) {
            statusCode = 400;
            return R"({"error":"Route distance and travel time must be valid numbers"})";
        }
    }

    if (request.method == "POST" && path == "/api/roads/block") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        std::string roadId;
        std::string blockedValue;
        if (!encodedValue(request.body, "roadId", roadId) ||
            !encodedValue(request.body, "blocked", blockedValue)) {
            statusCode = 400;
            return R"({"error":"Road ID and blocked state are required"})";
        }

        const EdgeNode* currentRoad = routeEngine_.graph().roadById(roadId);
        if (currentRoad == nullptr) {
            statusCode = 404;
            return R"({"error":"Road ID was not found"})";
        }

        RoadChange change;
        change.roadId = roadId;
        change.previousBlocked = currentRoad->blocked;
        change.previousTraffic = currentRoad->traffic;
        roadChangeHistory_.push(change);
        routeEngine_.graph().setRoadBlocked(
            roadId,
            blockedValue == "1" || blockedValue == "true" || blockedValue == "blocked"
        );
        deliverySystem_.recalculateActiveRoutes();
        return buildCityJson();
    }

    if (request.method == "POST" && path == "/api/roads/traffic") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        std::string roadId;
        std::string traffic;
        if (!encodedValue(request.body, "roadId", roadId) ||
            !encodedValue(request.body, "traffic", traffic) ||
            (traffic != "Low" && traffic != "Medium" && traffic != "High")) {
            statusCode = 400;
            return R"({"error":"A road ID and valid traffic level are required"})";
        }

        const EdgeNode* currentRoad = routeEngine_.graph().roadById(roadId);
        if (currentRoad == nullptr) {
            statusCode = 404;
            return R"({"error":"Road ID was not found"})";
        }

        RoadChange change;
        change.roadId = roadId;
        change.previousBlocked = currentRoad->blocked;
        change.previousTraffic = currentRoad->traffic;
        roadChangeHistory_.push(change);
        routeEngine_.graph().setRoadTraffic(roadId, Graph::parseTrafficLevel(traffic));
        deliverySystem_.recalculateActiveRoutes();
        return buildCityJson();
    }

    if (request.method == "POST" && path == "/api/roads/undo") {
        contentType = "application/json; charset=utf-8";
        std::string authorizationError;
        if (!authorize(request, AccountRole::Admin, statusCode, authorizationError)) {
            return authorizationError;
        }
        RoadChange change;
        if (!roadChangeHistory_.pop(change)) {
            statusCode = 400;
            return R"({"error":"There is no road change to undo"})";
        }
        routeEngine_.graph().setRoadBlocked(change.roadId, change.previousBlocked);
        routeEngine_.graph().setRoadTraffic(change.roadId, change.previousTraffic);
        deliverySystem_.recalculateActiveRoutes();
        return buildCityJson();
    }

    if (request.method == "POST") {
        statusCode = 404;
        contentType = "application/json; charset=utf-8";
        return R"({"error":"API endpoint not found"})";
    }

    return serveStaticFile(path, statusCode, contentType);
}

std::string LocalServer::serveStaticFile(
    const std::string& target,
    int& statusCode,
    std::string& contentType
) const {
    std::string relativeTarget = target == "/" ? "index.html" : target.substr(1);
    if (relativeTarget.find("..") != std::string::npos) {
        statusCode = 400;
        contentType = "application/json; charset=utf-8";
        return R"({"error":"Invalid path"})";
    }

    const std::filesystem::path filePath = projectRoot_ / "frontend" / relativeTarget;
    if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
        statusCode = 404;
        contentType = "application/json; charset=utf-8";
        return R"({"error":"Resource not found"})";
    }

    std::ifstream input(filePath, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    contentType = mimeTypeFor(filePath);
    statusCode = 200;
    return contents.str();
}

std::string LocalServer::buildCityJson() const {
    const Graph& graph = routeEngine_.graph();
    std::ostringstream json;
    json << "{\"city\":\"Nexa City\",\"locations\":[";
    bool first = true;
    for (int index = 0; index < graph.vertexCount(); ++index) {
        const Location* location = graph.location(index);
        if (location == nullptr) continue;
        if (!first) json << ',';
        first = false;
        json << "{\"id\":" << location->id
             << ",\"name\":\"" << jsonEscape(location->name) << "\""
             << ",\"type\":\"" << jsonEscape(location->type) << "\""
             << ",\"x\":" << location->x
             << ",\"y\":" << location->y << '}';
    }

    json << "],\"roads\":[";
    first = true;
    for (int from = 0; from < graph.vertexCount(); ++from) {
        for (const EdgeNode* edge = graph.edgesFrom(from); edge != nullptr; edge = edge->next) {
            if (from >= edge->destination) continue;
            if (!first) json << ',';
            first = false;
            json << "{\"id\":\"" << jsonEscape(edge->roadId) << "\""
                 << ",\"name\":\"" << jsonEscape(edge->roadName) << "\""
                 << ",\"from\":" << from
                 << ",\"to\":" << edge->destination
                 << ",\"distance\":" << std::fixed << std::setprecision(1) << edge->distanceKm
                 << ",\"baseTime\":" << edge->baseTimeMinutes
                 << ",\"traffic\":\"" << Graph::trafficName(edge->traffic) << "\""
                 << ",\"blocked\":" << (edge->blocked ? "true" : "false") << '}';
        }
    }

    json << "],\"customLocations\":" << networkManager_.customLocationCount()
         << ",\"customRoutes\":" << networkManager_.customRoadCount()
         << ",\"nextRouteId\":\"" << networkManager_.nextRoadIdPreview() << "\"}";
    return json.str();
}

std::string LocalServer::buildRidersJson(bool includePrivateDetails) const {
    std::ostringstream json;
    json << "{\"riders\":[";
    const DynamicArray<Rider>& riders = deliverySystem_.riders();
    bool first = true;
    for (std::size_t index = 0; index < riders.size(); ++index) {
        if (!first) json << ',';
        first = false;
        json << buildRiderJson(riders[index], includePrivateDetails);
    }

    json << "]}";
    return json.str();
}

std::string LocalServer::buildRiderJson(
    const Rider& rider,
    bool includePrivateDetails
) const {
    std::ostringstream json;
    json << "{\"id\":\"" << jsonEscape(rider.id) << "\""
         << ",\"name\":\"" << jsonEscape(rider.name) << "\""
         << ",\"locationId\":" << rider.currentLocationId
         << ",\"rating\":";
    if (rider.ratingCount > 0) {
        json << std::fixed << std::setprecision(1) << rider.rating;
    } else {
        json << "null";
    }
    json << ",\"ratingCount\":" << rider.ratingCount
         << ",\"vehicle\":\"" << jsonEscape(rider.vehicle) << "\""
         << ",\"available\":" << (rider.available ? "true" : "false")
         << ",\"completed\":" << rider.completedDeliveries;
    if (includePrivateDetails) {
        json << ",\"phone\":\"" << jsonEscape(rider.phone) << "\""
             << ",\"registration\":\"" << jsonEscape(rider.registration) << "\""
             << ",\"baseCharge\":" << rider.baseCharge
             << ",\"perKmCharge\":" << rider.perKmCharge
             << ",\"earnings\":" << rider.earnings
             << ",\"activeTrackingId\":\"" << jsonEscape(rider.activeTrackingId) << "\"";
    }
    json << '}';
    return json.str();
}

std::string LocalServer::buildRouteComparisonJson(int from, int to) const {
    const RouteResult minimumStopsRoute = routeEngine_.minimumStops(from, to);
    const RouteResult minimumDistanceRoute = routeEngine_.minimumDistance(from, to);

    const Location* start = routeEngine_.graph().location(from);
    const Location* destination = routeEngine_.graph().location(to);

    std::ostringstream json;
    json << "{\"from\":{\"id\":" << from << ",\"name\":\""
         << (start == nullptr ? "" : jsonEscape(start->name)) << "\"}"
         << ",\"to\":{\"id\":" << to << ",\"name\":\""
         << (destination == nullptr ? "" : jsonEscape(destination->name)) << "\"}"
         << ",\"minimumStops\":";
    appendRouteJson(json, minimumStopsRoute, routeEngine_.graph());
    json << ",\"minimumDistance\":";
    appendRouteJson(json, minimumDistanceRoute, routeEngine_.graph());
    json << '}';
    return json.str();
}

std::string LocalServer::buildBookingJson(const Booking& booking) const {
    const Graph& graph = routeEngine_.graph();
    const Location* pickup = graph.location(booking.pickupId);
    const Location* destination = graph.location(booking.destinationId);
    const Rider* rider = deliverySystem_.findRider(booking.assignedRiderId);

    std::ostringstream json;
    json << "{\"trackingId\":\"" << jsonEscape(booking.trackingId) << "\""
         << ",\"customerName\":\"" << jsonEscape(booking.customerName) << "\""
         << ",\"receiverName\":\"" << jsonEscape(booking.receiverName) << "\""
         << ",\"receiverPhone\":\"" << jsonEscape(booking.receiverPhone) << "\""
         << ",\"pickup\":{\"id\":" << booking.pickupId << ",\"name\":\""
         << (pickup == nullptr ? "" : jsonEscape(pickup->name)) << "\"}"
         << ",\"destination\":{\"id\":" << booking.destinationId << ",\"name\":\""
         << (destination == nullptr ? "" : jsonEscape(destination->name)) << "\"}"
         << ",\"weightKg\":" << std::fixed << std::setprecision(1) << booking.weightKg
         << ",\"priority\":\"" << DeliverySystem::priorityName(booking.priority) << "\""
         << ",\"mode\":\"" << DeliverySystem::modeName(booking.mode) << "\""
         << ",\"status\":\"" << DeliverySystem::statusName(booking.status) << "\""
         << ",\"estimatedFare\":" << booking.estimatedFare
         << ",\"acceptedFare\":" << booking.acceptedFare
         << ",\"createdAt\":\"" << jsonEscape(booking.createdAt) << "\""
         << ",\"statusChangedAt\":" << booking.statusChangedAtEpoch
         << ",\"nextStatusAt\":" << booking.nextStatusAtEpoch
         << ",\"customerRating\":";
    if (booking.customerRating > 0) json << booking.customerRating;
    else json << "null";
    json << ",\"route\":";
    appendRouteJson(json, booking.deliveryRoute, graph);

    json << ",\"rider\":";
    if (rider == nullptr) {
        json << "null";
    } else {
        json << buildRiderJson(*rider);
    }

    json << ",\"offers\":[";
    for (std::size_t index = 0; index < booking.offers.size(); ++index) {
        if (index > 0) json << ',';
        const RiderOffer& offer = booking.offers[index];
        json << "{\"riderId\":\"" << jsonEscape(offer.riderId) << "\""
             << ",\"riderName\":\"" << jsonEscape(offer.riderName) << "\""
             << ",\"vehicle\":\"" << jsonEscape(offer.vehicle) << "\""
             << ",\"rating\":";
        if (offer.rating > 0.0) json << offer.rating;
        else json << "null";
        json << ",\"pickupDistance\":" << offer.pickupDistanceKm
             << ",\"pickupMinutes\":" << offer.pickupMinutes
             << ",\"amount\":" << offer.amount << '}';
    }

    json << "],\"history\":[";
    bool first = true;
    booking.history.forEachForward([&json, &first](const StatusEvent& event) {
        if (!first) json << ',';
        first = false;
        json << "{\"status\":\"" << DeliverySystem::statusName(event.status) << "\""
             << ",\"time\":\"" << escapeJsonValue(event.time) << "\""
             << ",\"note\":\"" << escapeJsonValue(event.note) << "\"}";
    });
    json << "]}";
    return json.str();
}

std::string LocalServer::buildBookingsJson() const {
    std::ostringstream json;
    json << "{\"bookings\":[";
    bool first = true;
    deliverySystem_.bookings().forEach([this, &json, &first](const Booking& booking) {
        if (!first) json << ',';
        first = false;
        json << buildBookingJson(booking);
    });
    json << "]}";
    return json.str();
}

std::string LocalServer::buildDashboardJson(bool includeFinancialDetails) const {
    const DashboardSummary summary = deliverySystem_.dashboardSummary();
    std::ostringstream json;
    json << "{\"totalBookings\":" << summary.totalBookings
         << ",\"activeDeliveries\":" << summary.activeDeliveries
         << ",\"pendingBookings\":" << summary.pendingBookings
         << ",\"deliveredBookings\":" << summary.deliveredBookings
         << ",\"cancelledBookings\":" << summary.cancelledBookings
         << ",\"availableRiders\":" << summary.availableRiders;
    if (includeFinancialDetails) {
        json << ",\"totalRevenue\":" << std::fixed << std::setprecision(1) << summary.totalRevenue
             << ",\"riderPayouts\":" << summary.riderPayouts
             << ",\"platformProfit\":" << summary.platformProfit;
    }
    json << '}';
    return json.str();
}

std::string LocalServer::buildAnalyticsJson() const {
    const AnalyticsSnapshot snapshot = analyticsService_.createSnapshot(5);
    std::ostringstream json;
    json << std::fixed << std::setprecision(1)
         << "{\"generatedAt\":" << snapshot.generatedAtEpoch
         << ",\"network\":{\"totalLocations\":" << snapshot.network.totalLocations
         << ",\"reachableLocations\":" << snapshot.network.reachableLocations
         << ",\"totalRoads\":" << snapshot.network.totalRoads
         << ",\"openRoads\":" << snapshot.network.openRoads
         << ",\"blockedRoads\":" << snapshot.network.blockedRoads
         << ",\"totalRoadDistance\":" << snapshot.network.totalRoadDistanceKm
         << ",\"reachablePercentage\":" << snapshot.network.reachablePercentage
         << ",\"openRoadPercentage\":" << snapshot.network.openRoadPercentage
         << ",\"fullyConnected\":" << (snapshot.network.fullyConnected ? "true" : "false")
         << "},\"fleet\":{\"totalRiders\":" << snapshot.fleet.totalRiders
         << ",\"ratedRiders\":" << snapshot.fleet.ratedRiders
         << ",\"availableRiders\":" << snapshot.fleet.availableRiders
         << ",\"busyRiders\":" << snapshot.fleet.busyRiders
         << ",\"completedDeliveries\":" << snapshot.fleet.completedDeliveries
         << ",\"totalEarnings\":" << snapshot.fleet.totalEarnings
         << ",\"averageRating\":";
    if (snapshot.fleet.ratedRiders > 0) json << snapshot.fleet.averageRating;
    else json << "null";
    json << ",\"availabilityPercentage\":" << snapshot.fleet.availabilityPercentage
         << "},\"finance\":{\"grossRevenue\":" << snapshot.finance.grossRevenue
         << ",\"riderPayouts\":" << snapshot.finance.riderPayouts
         << ",\"platformProfit\":" << snapshot.finance.platformProfit
         << ",\"averageCompletedFare\":" << snapshot.finance.averageCompletedFare
         << ",\"profitMarginPercentage\":" << snapshot.finance.profitMarginPercentage
         << "},\"statusDistribution\":[";

    for (std::size_t index = 0; index < snapshot.statusDistribution.size(); ++index) {
        if (index > 0) json << ',';
        const DeliveryStatusMetric& metric = snapshot.statusDistribution[index];
        json << "{\"status\":\"" << jsonEscape(metric.status) << "\""
             << ",\"count\":" << metric.count
             << ",\"percentage\":" << metric.percentage << '}';
    }
    json << "],\"trafficDistribution\":[";
    for (std::size_t index = 0; index < snapshot.trafficDistribution.size(); ++index) {
        if (index > 0) json << ',';
        const TrafficMetric& metric = snapshot.trafficDistribution[index];
        json << "{\"level\":\"" << jsonEscape(metric.level) << "\""
             << ",\"roadCount\":" << metric.roadCount
             << ",\"distance\":" << metric.distanceKm
             << ",\"percentage\":" << metric.percentage << '}';
    }
    json << "],\"riderLeaderboard\":[";
    for (std::size_t index = 0; index < snapshot.riderLeaderboard.size(); ++index) {
        if (index > 0) json << ',';
        const RiderPerformance& rider = snapshot.riderLeaderboard[index];
        json << "{\"riderId\":\"" << jsonEscape(rider.riderId) << "\""
             << ",\"riderName\":\"" << jsonEscape(rider.riderName) << "\""
             << ",\"vehicle\":\"" << jsonEscape(rider.vehicle) << "\""
             << ",\"completed\":" << rider.completedDeliveries
             << ",\"earnings\":" << rider.earnings
             << ",\"rating\":";
        if (rider.rating > 0.0) json << rider.rating;
        else json << "null";
        json << ",\"available\":" << (rider.available ? "true" : "false")
             << ",\"score\":" << rider.performanceScore << '}';
    }
    json << "]}";
    return json.str();
}

std::string LocalServer::buildSessionJson(const AuthResult& result) const {
    std::ostringstream json;
    json << "{\"token\":\"" << jsonEscape(result.token) << "\""
         << ",\"accountId\":\"" << jsonEscape(result.accountId) << "\""
         << ",\"role\":\"" << AuthService::roleName(result.role) << "\""
         << ",\"expiresAt\":" << result.expiresAtEpoch << '}';
    return json.str();
}

std::string LocalServer::requestToken(const HttpRequest& request) const {
    std::string token;
    if (request.method == "POST" && encodedValue(request.body, "token", token)) {
        return token;
    }
    queryText(request.target, "token", token);
    return token;
}

bool LocalServer::authorize(
    const HttpRequest& request,
    AccountRole role,
    int& statusCode,
    std::string& errorJson,
    const std::string& requiredAccountId
) {
    const std::string token = requestToken(request);
    if (token.empty() || authService_.session(token) == nullptr) {
        statusCode = 401;
        errorJson = R"({"error":"Sign in to continue"})";
        return false;
    }
    const AuthSession* activeSession = authService_.session(token);
    if (activeSession == nullptr || activeSession->role != role) {
        statusCode = 403;
        errorJson = R"({"error":"This account cannot access the requested workspace"})";
        return false;
    }
    if (!authService_.validateSession(token, role, requiredAccountId)) {
        statusCode = 403;
        errorJson = R"({"error":"You can only change your own rider profile"})";
        return false;
    }
    return true;
}

std::string LocalServer::makeHttpResponse(
    int statusCode,
    const std::string& contentType,
    const std::string& body
) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << ' ' << statusText(statusCode) << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

std::string LocalServer::mimeTypeFor(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "text/javascript; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

std::string LocalServer::jsonEscape(const std::string& value) {
    return escapeJsonValue(value);
}
