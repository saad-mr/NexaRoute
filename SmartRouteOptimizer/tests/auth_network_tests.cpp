#include "AnalyticsService.h"
#include "AuthService.h"
#include "DeliverySystem.h"
#include "NetworkManager.h"
#include "RouteEngine.h"
#include "Security.h"
#include "Validation.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures = 0;
int checks = 0;

void expect(bool condition, const std::string& message) {
    ++checks;
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void expectEqual(
    const std::string& actual,
    const std::string& expected,
    const std::string& message
) {
    expect(actual == expected, message + " (actual: " + actual + ")");
}

void expectEqual(int actual, int expected, const std::string& message) {
    expect(actual == expected, message + " (actual: " + std::to_string(actual) + ")");
}

bool nearlyEqual(double left, double right, double tolerance = 0.001) {
    return std::fabs(left - right) <= tolerance;
}

std::filesystem::path projectRoot() {
    std::filesystem::path candidate = std::filesystem::current_path();
    for (int level = 0; level < 6; ++level) {
        if (std::filesystem::exists(candidate / "data" / "locations.csv") &&
            std::filesystem::exists(candidate / "data" / "riders.csv")) {
            return candidate;
        }
        candidate = candidate.parent_path();
    }
    return {};
}

std::filesystem::path uniqueRuntimeDirectory(const std::string& name) {
    const long long stamp = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    return projectRoot() / "build" / (name + "_" + std::to_string(stamp));
}

bool loadBaseRouteEngine(RouteEngine& engine) {
    const std::filesystem::path root = projectRoot();
    return !root.empty() && engine.load(
        root / "data" / "locations.csv",
        root / "data" / "roads.csv"
    );
}

bool initializeDelivery(RouteEngine& engine, DeliverySystem& delivery) {
    return delivery.initialize(
        &engine,
        projectRoot() / "data" / "riders.csv"
    );
}

void testSha256() {
    expectEqual(
        Security::sha256(""),
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855",
        "SHA-256 empty input vector"
    );
    expectEqual(
        Security::sha256("abc"),
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector"
    );
    expectEqual(
        Security::sha256("The quick brown fox jumps over the lazy dog"),
        "d7a8fbb307d7809469ca9abcb0082e4f"
        "8d5651e46d3cdb762d02d0bf37c9e592",
        "SHA-256 sentence vector"
    );

    Sha256 streaming;
    streaming.update("The quick ");
    streaming.update("brown fox ");
    streaming.update("jumps over the lazy dog");
    expectEqual(
        streaming.finalHex(),
        Security::sha256("The quick brown fox jumps over the lazy dog"),
        "streamed SHA-256 matches one-shot hashing"
    );
    expectEqual(streaming.finalHex(), Security::sha256(""), "digest resets after finalization");

    const std::string firstRandom = Security::randomHex(32);
    const std::string secondRandom = Security::randomHex(32);
    expectEqual(static_cast<int>(firstRandom.size()), 64, "32 random bytes become 64 hexadecimal characters");
    expect(firstRandom != secondRandom, "independent secure tokens should differ");
    for (const char character : firstRandom) {
        expect(
            (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f'),
            "random token contains only lowercase hexadecimal characters"
        );
    }

    expect(Security::constantTimeEquals("same-value", "same-value"), "constant-time equality accepts identical text");
    expect(!Security::constantTimeEquals("same-value", "same-Value"), "constant-time equality rejects changed text");
    expect(!Security::constantTimeEquals("short", "shorter"), "constant-time equality rejects changed length");
    expect(
        Security::derivePasswordHash("Strong@123", "a1b2", 3) ==
        Security::derivePasswordHash("Strong@123", "a1b2", 3),
        "password derivation is deterministic for equal inputs"
    );
    expect(
        Security::derivePasswordHash("Strong@123", "a1b2", 3) !=
        Security::derivePasswordHash("Strong@123", "ffff", 3),
        "password derivation changes when the salt changes"
    );
    expect(
        Security::derivePasswordHash("Strong@123", "a1b2", 3) !=
        Security::derivePasswordHash("Different@123", "a1b2", 3),
        "password derivation changes when the password changes"
    );
}

void testValidationUtilities() {
    expectEqual(Validation::trim("  Nexa Route  "), "Nexa Route", "trim removes outer whitespace");
    expectEqual(Validation::collapseSpaces("Nexa   Route\tCity"), "Nexa Route City", "spaces collapse consistently");
    expectEqual(Validation::normalizedName("  CENTRAL   Hub "), "central hub", "names normalize for lookup");
    expect(Validation::equalsIgnoreCase("Green Valley", " green  valley "), "name comparison ignores case and spacing");
    expect(!Validation::equalsIgnoreCase("Green Valley", "Green Village"), "different names remain distinct");

    expect(Validation::isSafePersistedText("Normal text", 20), "normal persistence text is accepted");
    expect(!Validation::isSafePersistedText("bad\tfield", 20), "tab-delimited injection is rejected");
    expect(!Validation::isSafePersistedText("bad\nfield", 20), "newline persistence injection is rejected");
    expect(!Validation::isSafePersistedText("", 20), "empty persisted text is rejected");

    expect(Validation::isIdentifier("RD-109", 4, 20), "rider identifier format is accepted");
    expect(Validation::isIdentifier("route_12", 2, 20), "underscore identifier format is accepted");
    expect(!Validation::isIdentifier("bad id", 2, 20), "identifier spaces are rejected");
    expect(!Validation::isIdentifier("x", 2, 20), "identifier minimum length is enforced");

    expect(Validation::isPersonName("Areeba Khan"), "person name is accepted");
    expect(Validation::isPersonName("Maryam O'Neil"), "apostrophe in person name is accepted");
    expect(!Validation::isPersonName("12345"), "numeric person name is rejected");
    expect(!Validation::isPersonName("A"), "one-character person name is rejected");
    expect(!Validation::isPersonName("Name<script>"), "markup characters in person name are rejected");

    expect(Validation::isPhoneNumber("0300-1234567"), "Pakistani phone format is accepted");
    expect(Validation::isPhoneNumber("+92 (300) 1234567"), "international phone format is accepted");
    expect(!Validation::isPhoneNumber("12345"), "short phone number is rejected");
    expect(!Validation::isPhoneNumber("0300-CALL-NOW"), "phone letters are rejected");

    expect(Validation::isVehicleName("Bike"), "bike vehicle is accepted");
    expect(Validation::isVehicleName("Pickup"), "pickup vehicle is accepted");
    expect(!Validation::isVehicleName("Truck"), "unsupported vehicle is rejected");
    expect(Validation::isVehicleRegistration("NXR-109"), "vehicle registration is accepted");
    expect(!Validation::isVehicleRegistration("ONLYLETTERS"), "registration requires a digit");
    expect(!Validation::isVehicleRegistration("123456"), "registration requires a letter");

    expect(Validation::isLocationName("Block 7, North"), "location name punctuation is accepted");
    expect(Validation::isLocationName("Airport (Cargo)"), "parenthesized location name is accepted");
    expect(!Validation::isLocationName("<script>"), "unsafe location punctuation is rejected");
    expect(Validation::isLocationType("Distribution Center"), "location type is accepted");
    expect(!Validation::isLocationType("Type 2"), "numeric location type is rejected");
    expect(Validation::isRouteName("Canal Express / Link 2"), "named route punctuation is accepted");
    expect(!Validation::isRouteName("#"), "meaningless route name is rejected");

    expect(Validation::isMapCoordinate(70, 60), "minimum map coordinate is accepted");
    expect(Validation::isMapCoordinate(1130, 650), "maximum map coordinate is accepted");
    expect(!Validation::isMapCoordinate(69, 60), "out-of-range x coordinate is rejected");
    expect(!Validation::isMapCoordinate(70, 651), "out-of-range y coordinate is rejected");
    expect(Validation::isDistance(0.1), "minimum route distance is accepted");
    expect(Validation::isDistance(100.0), "maximum route distance is accepted");
    expect(!Validation::isDistance(0.0), "zero route distance is rejected");
    expect(!Validation::isDistance(100.1), "excessive route distance is rejected");
    expect(Validation::isTravelTime(1), "minimum travel time is accepted");
    expect(Validation::isTravelTime(180), "maximum travel time is accepted");
    expect(!Validation::isTravelTime(0), "zero travel time is rejected");
    expect(!Validation::isTravelTime(181), "excessive travel time is rejected");
    expect(Validation::isMoney(0.0, 2000.0), "zero base charge is accepted");
    expect(!Validation::isMoney(-1.0, 2000.0), "negative charge is rejected");

    expectEqual(Validation::passwordError("short"), "Password must contain 8 to 72 characters.", "password length policy");
    expectEqual(Validation::passwordError("lowercase@1"), "Password must include an uppercase letter.", "uppercase password policy");
    expectEqual(Validation::passwordError("UPPERCASE@1"), "Password must include a lowercase letter.", "lowercase password policy");
    expectEqual(Validation::passwordError("NoNumber@"), "Password must include a number.", "number password policy");
    expectEqual(Validation::passwordError("NoSymbol1"), "Password must include a symbol.", "symbol password policy");
    expect(Validation::passwordError("Secure@123").empty(), "strong password is accepted");
}

void testAuthenticationWithoutPersistence() {
    RouteEngine engine;
    DeliverySystem delivery;
    expect(loadBaseRouteEngine(engine), "base graph loads for authentication test");
    expect(initializeDelivery(engine, delivery), "empty rider store loads for authentication test");
    expectEqual(static_cast<int>(delivery.riders().size()), 0, "clean delivery system has no registered riders");

    AuthService auth;
    expect(auth.initialize({}, false), "authentication service initializes without persistence");
    expect(auth.ready(), "authentication service reports ready");
    expectEqual(static_cast<int>(auth.accountCount()), 1, "clean authentication state contains only the admin account");
    expect(!auth.credentialExists("RD-101", AccountRole::Rider), "no rider credential is fabricated");
    expect(auth.credentialExists("ADMIN", AccountRole::Admin), "admin credential lookup is case-insensitive");
    expect(!auth.credentialExists("RD-999", AccountRole::Rider), "unknown rider credential does not exist");

    std::string registrationError;
    expect(auth.registerRiderCredential("RD-101", "Rider@One1", registrationError), "first rider creates a credential");
    expect(auth.registerRiderCredential("RD-102", "Rider@Two2", registrationError), "second rider creates a credential");
    expect(auth.credentialExists("rd-101", AccountRole::Rider), "registered rider lookup is case-insensitive");

    const AuthResult wrongRider = auth.loginRider("RD-101", "Wrong@123");
    expect(!wrongRider.success, "wrong rider password is rejected");
    expect(wrongRider.error.find("incorrect") != std::string::npos, "wrong rider login returns a generic error");

    const AuthResult rider = auth.loginRider(" rd-101 ", "Rider@One1");
    expect(rider.success, "registered rider can log in with the chosen password");
    expectEqual(rider.accountId, "RD-101", "rider ID is canonicalized");
    expect(rider.role == AccountRole::Rider, "rider session has rider role");
    expectEqual(static_cast<int>(rider.token.size()), 64, "rider session token has 256-bit hexadecimal form");
    expect(rider.expiresAtEpoch > 0, "rider session has an expiration time");
    expect(auth.validateSession(rider.token, AccountRole::Rider), "rider session validates for rider role");
    expect(auth.validateSession(rider.token, AccountRole::Rider, "rd-101"), "rider session validates for its own account");
    expect(!auth.validateSession(rider.token, AccountRole::Rider, "RD-102"), "rider session cannot edit another rider");
    expect(!auth.validateSession(rider.token, AccountRole::Admin), "rider session cannot access admin role");
    expect(auth.session(rider.token) != nullptr, "active rider session can be retrieved");

    const AuthResult wrongAdmin = auth.loginAdmin("admin", "Wrong@2026");
    expect(!wrongAdmin.success, "wrong admin password is rejected");
    const AuthResult admin = auth.loginAdmin(" ADMIN ", AuthService::defaultAdminPassword());
    expect(admin.success, "admin can log in with the project password");
    expectEqual(admin.accountId, "admin", "admin username is canonicalized");
    expect(admin.role == AccountRole::Admin, "admin session has admin role");
    expect(auth.validateSession(admin.token, AccountRole::Admin), "admin session validates for admin role");
    expect(!auth.validateSession(admin.token, AccountRole::Rider), "admin session is not accepted as rider session");
    expectEqual(static_cast<int>(auth.activeSessionCount()), 2, "two successful logins create two active sessions");

    expect(!auth.registerRiderCredential("x", "Secure@123", registrationError), "short rider ID is rejected");
    expect(!registrationError.empty(), "invalid rider ID returns a registration error");
    expect(!auth.registerRiderCredential("RD-109", "weakpass", registrationError), "weak rider password is rejected");
    expect(!registrationError.empty(), "weak password returns a registration error");
    expect(auth.registerRiderCredential("rd-109", "Secure@109", registrationError), "new rider credential is registered");
    expect(auth.credentialExists("RD-109", AccountRole::Rider), "new rider credential can be found");
    expect(!auth.registerRiderCredential("RD-109", "Another@109", registrationError), "duplicate rider credential is rejected");

    const AuthResult newRider = auth.loginRider("RD-109", "Secure@109");
    expect(newRider.success, "new rider can log in");
    std::string passwordError;
    expect(
        !auth.changePassword(newRider.token, "Incorrect@109", "Updated@109", passwordError),
        "password change rejects an incorrect current password"
    );
    expect(
        !auth.changePassword(newRider.token, "Secure@109", "weakpass", passwordError),
        "password change enforces the password policy"
    );
    expect(
        !auth.changePassword(newRider.token, "Secure@109", "Secure@109", passwordError),
        "password change requires a different new password"
    );
    expect(
        auth.changePassword(newRider.token, "Secure@109", "Updated@109", passwordError),
        "rider can change to a strong new password"
    );
    expect(!auth.loginRider("RD-109", "Secure@109").success, "old password stops working after change");
    expect(auth.loginRider("RD-109", "Updated@109").success, "new password works after change");

    expect(auth.logout(rider.token), "active rider session logs out");
    expect(!auth.logout(rider.token), "same session cannot log out twice");
    expect(!auth.validateSession(rider.token, AccountRole::Rider), "logged-out rider session no longer validates");
    expect(auth.session(rider.token) == nullptr, "logged-out session is not returned");

    expect(auth.removeRiderCredential("RD-109"), "rider credential can be disabled");
    expect(!auth.loginRider("RD-109", "Updated@109").success, "disabled rider credential cannot log in");

    for (int attempt = 0; attempt < 5; ++attempt) {
        expect(!auth.loginRider("RD-102", "Wrong@123").success, "failed login contributes to temporary lock");
    }
    const AuthResult locked = auth.loginRider("RD-102", "Rider@Two2");
    expect(!locked.success, "correct password is delayed while account is temporarily locked");
    expect(locked.error.find("locked") != std::string::npos, "temporary lock is explained without exposing secrets");
}

void testAuthenticationPersistence() {
    RouteEngine engine;
    DeliverySystem delivery;
    expect(loadBaseRouteEngine(engine), "base graph loads for credential persistence test");
    expect(initializeDelivery(engine, delivery), "empty rider store loads for credential persistence test");

    const std::filesystem::path runtime = uniqueRuntimeDirectory("auth_runtime");
    std::error_code filesystemError;
    std::filesystem::create_directories(runtime, filesystemError);
    expect(!filesystemError, "temporary authentication directory is created");

    {
        AuthService first;
        expect(first.initialize(runtime, true), "persistent authentication service initializes");
        std::string error;
        expect(first.registerRiderCredential("RD-103", "Initial@103", error), "persistent rider credential is created");
        const AuthResult login = first.loginRider("RD-103", "Initial@103");
        expect(login.success, "persistent registered rider logs in");
        expect(first.changePassword(login.token, "Initial@103", "Persist@103", error), "persistent password changes");
        expect(first.registerRiderCredential("RD-199", "Persist@199", error), "new persistent credential is created");
        expect(std::filesystem::exists(runtime / "accounts.db"), "credential database is written");
        expect(std::filesystem::exists(runtime / "auth_audit.db"), "authentication audit log is written");
    }

    {
        AuthService second;
        expect(second.initialize(runtime, true), "authentication service reloads saved credentials");
        expect(!second.loginRider("RD-103", "Initial@103").success, "reloaded account rejects old password");
        expect(second.loginRider("RD-103", "Persist@103").success, "reloaded account accepts changed password");
        expect(second.loginRider("RD-199", "Persist@199").success, "reloaded custom rider credential works");
        expect(second.loginAdmin("admin", AuthService::defaultAdminPassword()).success, "admin credential survives reload");
        expectEqual(
            static_cast<int>(second.accountCount()),
            3,
            "reloaded account count contains admin and two registered riders"
        );
    }

    std::filesystem::remove_all(runtime, filesystemError);
    expect(!std::filesystem::exists(runtime), "temporary authentication directory is cleaned up");
}

AddNamedDestinationRequest validDestination() {
    AddNamedDestinationRequest request;
    request.locationName = "Green Valley Depot";
    request.locationType = "Depot";
    request.connectFromId = 0;
    request.routeName = "Green Valley Road";
    request.distanceKm = 4.7;
    request.baseTimeMinutes = 9;
    request.traffic = TrafficLevel::Low;
    return request;
}

void testNetworkManagerWithoutPersistence() {
    NetworkManager unavailable;
    expect(!unavailable.initialize(nullptr, {}, false), "network manager rejects a missing route engine");
    expect(!unavailable.ready(), "network manager remains unavailable without a graph");

    RouteEngine engine;
    expect(loadBaseRouteEngine(engine), "base graph loads for network manager test");
    const int baseLocations = engine.graph().vertexCount();
    const int baseRoads = engine.graph().roadCount();
    NetworkManager manager;
    expect(manager.initialize(&engine, {}, false), "network manager initializes without persistence");
    expect(manager.ready(), "network manager reports ready");
    expectEqual(manager.customLocationCount(), 0, "custom location count starts at zero");
    expectEqual(manager.customRoadCount(), 0, "custom road count starts at zero");
    expectEqual(manager.resolveLocationId("0"), 0, "location resolves by numeric ID");
    expectEqual(manager.resolveLocationId(" Central Courier Hub "), 0, "location resolves by trimmed name");
    expectEqual(manager.resolveLocationId("central   courier hub"), 0, "location resolves case-insensitively");
    expectEqual(manager.resolveLocationId("missing place"), -1, "unknown location name does not resolve");
    expectEqual(manager.resolveLocationId("999"), -1, "out-of-range numeric location does not resolve");
    expectEqual(manager.nextRoadIdPreview(), "NR-001", "first generated route ID preview is stable");

    AddNamedDestinationRequest invalid = validDestination();
    invalid.locationName = "x";
    expect(!manager.addNamedDestination(invalid).success, "short destination name is rejected");
    invalid = validDestination();
    invalid.locationName = "Central Courier Hub";
    expect(!manager.addNamedDestination(invalid).success, "duplicate destination name is rejected");
    invalid = validDestination();
    invalid.locationType = "Type 2";
    expect(!manager.addNamedDestination(invalid).success, "invalid destination type is rejected");
    invalid = validDestination();
    invalid.connectFromId = 999;
    expect(!manager.addNamedDestination(invalid).success, "invalid destination anchor is rejected");
    invalid = validDestination();
    invalid.routeName = "#";
    expect(!manager.addNamedDestination(invalid).success, "invalid connecting route name is rejected");
    invalid = validDestination();
    invalid.distanceKm = 0.0;
    expect(!manager.addNamedDestination(invalid).success, "zero connecting distance is rejected");
    invalid = validDestination();
    invalid.baseTimeMinutes = 181;
    expect(!manager.addNamedDestination(invalid).success, "excessive connecting time is rejected");
    expectEqual(engine.graph().vertexCount(), baseLocations, "invalid destination attempts do not alter graph");

    const NetworkChangeResult added = manager.addNamedDestination(validDestination());
    expect(added.success, "valid named destination is added");
    expectEqual(added.locationId, baseLocations, "new destination receives next sequential ID");
    expectEqual(added.roadId, "NR-001", "new destination receives generated route ID");
    expectEqual(engine.graph().vertexCount(), baseLocations + 1, "graph grows by one location");
    expectEqual(engine.graph().roadCount(), baseRoads + 1, "graph grows by one connecting route");
    expectEqual(manager.customLocationCount(), 1, "custom location count increments");
    expectEqual(manager.customRoadCount(), 1, "custom route count increments");
    expectEqual(manager.resolveLocationId("Green Valley Depot"), added.locationId, "new location resolves by name");
    expectEqual(manager.resolveLocationId("green valley depot"), added.locationId, "new location lookup ignores case");
    const Location* location = engine.graph().location(added.locationId);
    expect(location != nullptr, "new location is available in graph");
    if (location != nullptr) {
        expectEqual(location->name, "Green Valley Depot", "new location name is normalized and stored");
        expectEqual(location->type, "Depot", "new location type is stored");
        expect(Validation::isMapCoordinate(location->x, location->y), "new location receives a valid map coordinate");
    }
    const EdgeNode* connectingRoad = engine.graph().roadById(added.roadId);
    expect(connectingRoad != nullptr, "new connecting route is available in graph");
    if (connectingRoad != nullptr) {
        expectEqual(connectingRoad->roadName, "Green Valley Road", "user-defined route name is stored");
        expect(nearlyEqual(connectingRoad->distanceKm, 4.7), "connecting route distance is stored");
        expectEqual(connectingRoad->baseTimeMinutes, 9, "connecting route time is stored");
    }
    const RouteResult destinationRoute = engine.minimumDistance(0, added.locationId);
    expect(destinationRoute.found, "route engine reaches the new destination");
    expect(nearlyEqual(destinationRoute.distanceKm, 4.7), "route engine uses new route distance");
    expectEqual(destinationRoute.stops, 1, "new destination is one road from its anchor");
    expect(!manager.addNamedDestination(validDestination()).success, "same custom destination cannot be added twice");

    AddExistingRoadRequest existingRoad;
    existingRoad.routeName = "Harbor Bypass";
    existingRoad.fromId = 11;
    existingRoad.toId = 14;
    existingRoad.distanceKm = 8.4;
    existingRoad.baseTimeMinutes = 14;
    existingRoad.traffic = TrafficLevel::Medium;
    const NetworkChangeResult roadAdded = manager.addRoad(existingRoad);
    expect(roadAdded.success, "named road between existing locations is added");
    expectEqual(roadAdded.roadId, "NR-002", "second generated route ID is sequential");
    expectEqual(manager.nextRoadIdPreview(), "NR-003", "next route preview advances");
    expectEqual(manager.customRoadCount(), 2, "custom route count includes both additions");
    const EdgeNode* existing = engine.graph().roadById(roadAdded.roadId);
    expect(existing != nullptr, "new existing-location road is indexed");
    if (existing != nullptr) {
        expectEqual(existing->roadName, "Harbor Bypass", "existing-location road stores its user name");
        expect(existing->traffic == TrafficLevel::Medium, "existing-location road stores traffic level");
    }
    expect(!manager.addRoad(existingRoad).success, "duplicate direct connection is rejected");
    existingRoad.fromId = 11;
    existingRoad.toId = 11;
    expect(!manager.addRoad(existingRoad).success, "self-connecting route is rejected");
    existingRoad.fromId = -1;
    existingRoad.toId = 14;
    expect(!manager.addRoad(existingRoad).success, "route with invalid endpoint is rejected");
}

void testNetworkPersistence() {
    const std::filesystem::path runtime = uniqueRuntimeDirectory("network_runtime");
    std::error_code filesystemError;
    std::filesystem::create_directories(runtime, filesystemError);
    expect(!filesystemError, "temporary network directory is created");

    int customLocationId = -1;
    {
        RouteEngine firstEngine;
        expect(loadBaseRouteEngine(firstEngine), "first graph loads for network persistence");
        NetworkManager first;
        expect(first.initialize(&firstEngine, runtime, true), "persistent network manager initializes");
        const NetworkChangeResult location = first.addNamedDestination(validDestination());
        expect(location.success, "persistent destination is added");
        customLocationId = location.locationId;

        AddExistingRoadRequest road;
        road.routeName = "Harbor Bypass";
        road.fromId = 11;
        road.toId = 14;
        road.distanceKm = 8.4;
        road.baseTimeMinutes = 14;
        road.traffic = TrafficLevel::High;
        expect(first.addRoad(road).success, "persistent existing-location route is added");
        expect(std::filesystem::exists(runtime / "added_locations.db"), "custom location database is written");
        expect(std::filesystem::exists(runtime / "added_roads.db"), "custom route database is written");
        expect(std::filesystem::exists(runtime / "network_audit.db"), "network audit database is written");
    }

    {
        RouteEngine secondEngine;
        expect(loadBaseRouteEngine(secondEngine), "second graph loads for network persistence");
        NetworkManager second;
        expect(second.initialize(&secondEngine, runtime, true), "saved custom network reloads");
        expectEqual(second.customLocationCount(), 1, "saved location count reloads");
        expectEqual(second.customRoadCount(), 2, "saved route count reloads");
        expectEqual(second.resolveLocationId("Green Valley Depot"), customLocationId, "saved location resolves after restart");
        expectEqual(second.nextRoadIdPreview(), "NR-003", "generated route sequence survives restart");
        const EdgeNode* destinationRoad = secondEngine.graph().roadById("NR-001");
        const EdgeNode* bypass = secondEngine.graph().roadById("NR-002");
        expect(destinationRoad != nullptr, "saved destination route reloads");
        expect(bypass != nullptr, "saved existing-location route reloads");
        if (destinationRoad != nullptr) {
            expectEqual(destinationRoad->roadName, "Green Valley Road", "saved destination route name reloads");
        }
        if (bypass != nullptr) {
            expectEqual(bypass->roadName, "Harbor Bypass", "saved bypass route name reloads");
            expect(bypass->traffic == TrafficLevel::High, "saved bypass traffic reloads");
        }
        const RouteResult route = secondEngine.minimumDistance(0, customLocationId);
        expect(route.found, "reloaded destination participates in route calculation");
        expect(nearlyEqual(route.distanceKm, 4.7), "reloaded destination route keeps its distance");
    }

    std::filesystem::remove_all(runtime, filesystemError);
    expect(!std::filesystem::exists(runtime), "temporary network directory is cleaned up");
}

void testAnalyticsService() {
    AnalyticsService unavailable;
    expect(!unavailable.initialize(nullptr, nullptr), "analytics service rejects missing dependencies");
    expect(!unavailable.ready(), "analytics service remains unavailable without dependencies");
    const AnalyticsSnapshot empty = unavailable.createSnapshot();
    expectEqual(empty.network.totalLocations, 0, "unavailable analytics snapshot is empty");
    expectEqual(static_cast<int>(empty.riderLeaderboard.size()), 0, "unavailable leaderboard is empty");

    RouteEngine engine;
    DeliverySystem delivery;
    expect(loadBaseRouteEngine(engine), "base graph loads for analytics test");
    expect(initializeDelivery(engine, delivery), "empty rider store loads for analytics test");

    AnalyticsService analytics;
    expect(analytics.initialize(&delivery, &engine.graph()), "analytics service initializes");
    expect(analytics.ready(), "analytics service reports ready");
    const AnalyticsSnapshot initial = analytics.createSnapshot(5);
    expect(initial.generatedAtEpoch > 0, "analytics snapshot includes generation time");
    expectEqual(initial.dashboard.totalBookings, 0, "initial analytics has no bookings");
    expectEqual(initial.network.totalLocations, 15, "analytics counts base locations");
    expectEqual(initial.network.totalRoads, 32, "analytics counts unique base roads");
    expectEqual(initial.network.openRoads + initial.network.blockedRoads, 32, "open and blocked road totals are consistent");
    expectEqual(initial.network.reachableLocations, 15, "all base locations are reachable");
    expect(initial.network.fullyConnected, "base network is fully connected");
    expect(nearlyEqual(initial.network.reachablePercentage, 100.0), "base network has full reachability percentage");
    expect(initial.network.totalRoadDistanceKm > 0.0, "analytics sums unique road distances");
    expectEqual(initial.fleet.totalRiders, 0, "clean analytics starts with no riders");
    expectEqual(initial.fleet.ratedRiders, 0, "clean analytics starts with no ratings");
    expectEqual(initial.fleet.availableRiders, 0, "clean analytics has no available riders");
    expectEqual(initial.fleet.busyRiders, 0, "clean analytics has no busy riders");
    expect(nearlyEqual(initial.fleet.averageRating, 0.0), "clean analytics has no fabricated rating average");
    expect(nearlyEqual(initial.fleet.availabilityPercentage, 0.0), "empty fleet availability is zero");
    expectEqual(static_cast<int>(initial.statusDistribution.size()), 9, "analytics includes every delivery status");
    for (std::size_t index = 0; index < initial.statusDistribution.size(); ++index) {
        expectEqual(initial.statusDistribution[index].count, 0, "empty status distribution contains zero counts");
    }
    expectEqual(static_cast<int>(initial.trafficDistribution.size()), 3, "analytics includes low, medium, and high traffic");
    int trafficRoadTotal = 0;
    double trafficPercentageTotal = 0.0;
    for (std::size_t index = 0; index < initial.trafficDistribution.size(); ++index) {
        trafficRoadTotal += initial.trafficDistribution[index].roadCount;
        trafficPercentageTotal += initial.trafficDistribution[index].percentage;
    }
    expectEqual(trafficRoadTotal, 32, "traffic distribution counts every unique road once");
    expect(nearlyEqual(trafficPercentageTotal, 100.0), "traffic percentages total one hundred percent");
    expectEqual(static_cast<int>(initial.riderLeaderboard.size()), 0, "empty fleet produces an empty leaderboard");
    for (std::size_t index = 1; index < initial.riderLeaderboard.size(); ++index) {
        expect(
            initial.riderLeaderboard[index - 1].performanceScore >=
            initial.riderLeaderboard[index].performanceScore,
            "leaderboard is ordered from highest to lowest performance score"
        );
    }
    expectEqual(
        static_cast<int>(analytics.createSnapshot(0).riderLeaderboard.size()),
        0,
        "zero leaderboard limit returns no riders"
    );
    expectEqual(
        static_cast<int>(analytics.createSnapshot(50).riderLeaderboard.size()),
        0,
        "empty leaderboard remains empty for a large limit"
    );

    RegisterRiderRequest riderRequest;
    riderRequest.name = "Analytics Rider";
    riderRequest.phone = "03005550120";
    riderRequest.locationId = 5;
    riderRequest.vehicle = "Bike";
    riderRequest.registration = "ANL-120";
    riderRequest.baseCharge = 130.0;
    riderRequest.perKmCharge = 29.0;
    std::string riderError;
    Rider* registeredRider = delivery.registerRider(riderRequest, riderError);
    expect(registeredRider != nullptr, "analytics rider is registered dynamically");
    if (registeredRider != nullptr) {
        expect(nearlyEqual(registeredRider->rating, 0.0), "new rider begins without a fabricated rating");
        expectEqual(registeredRider->currentLocationId, 5, "new rider keeps the selected starting location");
    }
    const AnalyticsSnapshot registeredFleet = analytics.createSnapshot(5);
    expectEqual(registeredFleet.fleet.totalRiders, 1, "fleet analytics updates after registration");
    expectEqual(registeredFleet.fleet.availableRiders, 1, "registered rider appears as available");
    expectEqual(registeredFleet.fleet.ratedRiders, 0, "unrated rider is excluded from rating totals");
    expect(nearlyEqual(registeredFleet.fleet.averageRating, 0.0), "unrated rider does not create an average rating");
    expectEqual(static_cast<int>(registeredFleet.riderLeaderboard.size()), 1, "leaderboard updates after registration");

    CreateBookingRequest request;
    request.customerName = "Muhammad Saad";
    request.receiverName = "Hanzala Khan";
    request.receiverPhone = "0300-1234567";
    request.pickupId = 5;
    request.destinationId = 7;
    request.weightKg = 2.5;
    request.priority = ParcelPriority::Express;
    request.mode = BookingMode::QuickAssign;
    const BookingCreationResult created = delivery.createBooking(request);
    expect(created.success && created.booking != nullptr, "analytics test booking is created");
    if (created.booking != nullptr) {
        std::string error;
        for (int step = 0; step < 4; ++step) {
            expect(delivery.advanceBooking(created.booking->trackingId, error), "analytics test booking advances through lifecycle");
        }
        expect(delivery.rateDelivery(created.booking->trackingId, 4, error), "completed delivery accepts customer feedback");
        expect(!delivery.rateDelivery(created.booking->trackingId, 5, error), "completed delivery rejects duplicate feedback");
    }

    const AnalyticsSnapshot completed = analytics.createSnapshot();
    expectEqual(completed.dashboard.totalBookings, 1, "analytics observes completed booking total");
    expectEqual(completed.dashboard.deliveredBookings, 1, "analytics observes delivered booking total");
    expectEqual(completed.statusDistribution[7].count, 1, "delivered status distribution increments");
    expect(nearlyEqual(completed.statusDistribution[7].percentage, 100.0), "delivered status becomes one hundred percent");
    expect(completed.finance.grossRevenue > 0.0, "completed delivery produces gross revenue");
    expect(completed.finance.riderPayouts > 0.0, "completed delivery produces rider payout");
    expect(completed.finance.platformProfit > 0.0, "completed delivery produces platform profit");
    expect(nearlyEqual(completed.finance.averageCompletedFare, completed.finance.grossRevenue), "single completed fare equals average fare");
    expect(nearlyEqual(completed.finance.profitMarginPercentage, 22.0), "platform margin reflects the configured rider share");
    expectEqual(completed.fleet.completedDeliveries, registeredFleet.fleet.completedDeliveries + 1, "fleet completion total updates");
    expectEqual(completed.fleet.ratedRiders, 1, "customer feedback adds one genuinely rated rider");
    expect(nearlyEqual(completed.fleet.averageRating, 4.0), "fleet rating average uses submitted feedback");

    const int blockedBefore = engine.graph().blockedRoadCount();
    expect(engine.graph().setRoadBlocked("R22", true), "analytics test blocks a road");
    const AnalyticsSnapshot changedNetwork = analytics.createSnapshot();
    expectEqual(changedNetwork.network.blockedRoads, blockedBefore + 1, "network analytics observes road block");
    expectEqual(changedNetwork.network.openRoads, 32 - changedNetwork.network.blockedRoads, "network open-road count updates");
    expect(changedNetwork.network.openRoadPercentage < initial.network.openRoadPercentage, "road block lowers open-road percentage");
}
}

int main() {
    if (projectRoot().empty()) {
        std::cerr << "Could not locate project data files.\n";
        return 1;
    }

    testSha256();
    testValidationUtilities();
    testAuthenticationWithoutPersistence();
    testAuthenticationPersistence();
    testNetworkManagerWithoutPersistence();
    testNetworkPersistence();
    testAnalyticsService();

    if (failures != 0) {
        std::cerr << failures << " of " << checks << " checks failed.\n";
        return 1;
    }
    std::cout << "Authentication, validation, security, and custom network tests passed ("
              << checks << " checks).\n";
    return 0;
}
