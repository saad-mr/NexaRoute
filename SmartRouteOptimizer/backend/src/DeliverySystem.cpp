#include "DeliverySystem.h"

#include "BinarySearch.h"
#include "CircularQueue.h"
#include "MinHeap.h"
#include "Validation.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
struct CsvRow {
    std::string values[20];
    int count = 0;
    const std::string& operator[](int index) const { return values[index]; }
};

CsvRow splitLine(const std::string& line, char delimiter) {
    CsvRow row;
    std::size_t start = 0;
    while (row.count < 20) {
        const std::size_t separator = line.find(delimiter, start);
        if (separator == std::string::npos) {
            row.values[row.count++] = line.substr(start);
            break;
        }
        row.values[row.count++] = line.substr(start, separator - start);
        start = separator + 1;
        if (start == line.size() && row.count < 20) {
            row.values[row.count++] = "";
            break;
        }
    }
    return row;
}

CsvRow splitCsvLine(const std::string& line) {
    return splitLine(line, ',');
}

struct RiderCandidate {
    int riderIndex = -1;
    double distanceKm = 0.0;
    double pickupMinutes = 0.0;
    double rating = 0.0;

    bool operator<(const RiderCandidate& other) const {
        if (std::fabs(distanceKm - other.distanceKm) > 0.001) {
            return distanceKm < other.distanceKm;
        }
        return rating > other.rating;
    }
};

double roundToFive(double value) {
    return std::round(value / 5.0) * 5.0;
}
}

DeliverySystem::DeliverySystem()
    : routeEngine_(nullptr), ready_(false), riders_(), bookings_(),
      trackingIndex_(101), completedRecords_(), bookingSequence_(0), riderSequence_(100),
      runtimeDirectory_(), persistenceEnabled_(false) {
}

bool DeliverySystem::initialize(
    RouteEngine* routeEngine,
    const std::filesystem::path& ridersFile,
    const std::filesystem::path& runtimeDirectory
) {
    routeEngine_ = routeEngine;
    runtimeDirectory_ = runtimeDirectory;
    persistenceEnabled_ = !runtimeDirectory_.empty();
    ready_ = routeEngine_ != nullptr && routeEngine_->graph().vertexCount() > 0 && loadRiders(ridersFile);
    if (ready_ && persistenceEnabled_) {
        std::error_code error;
        std::filesystem::create_directories(runtimeDirectory_, error);
        if (!error) {
            loadRuntimeData();
        } else {
            persistenceEnabled_ = false;
        }
    }
    return ready_;
}

bool DeliverySystem::ready() const {
    return ready_;
}

BookingCreationResult DeliverySystem::createBooking(const CreateBookingRequest& request) {
    BookingCreationResult result;
    if (!ready_) {
        result.error = "The delivery system is unavailable.";
        return result;
    }

    if (!validateRequest(request, result.error)) {
        return result;
    }

    RouteResult route = routeEngine_->minimumDistance(request.pickupId, request.destinationId);
    if (!route.found) {
        result.error = "No unblocked route is available between the selected locations.";
        return result;
    }

    Booking booking;
    booking.trackingId = nextTrackingId();
    booking.customerName = Validation::collapseSpaces(Validation::trim(request.customerName));
    booking.receiverName = Validation::collapseSpaces(Validation::trim(request.receiverName));
    booking.receiverPhone = Validation::collapseSpaces(Validation::trim(request.receiverPhone));
    booking.pickupId = request.pickupId;
    booking.destinationId = request.destinationId;
    booking.weightKg = request.weightKg;
    booking.priority = request.priority;
    booking.mode = request.mode;
    booking.deliveryRoute = std::move(route);
    booking.estimatedFare = calculateFare(request, booking.deliveryRoute);
    booking.acceptedFare = booking.estimatedFare;
    booking.createdAt = currentTimeText();

    addStatus(booking, DeliveryStatus::Requested, "Parcel request created by customer.");
    addStatus(booking, DeliveryStatus::SearchingForRider, "Searching the rider network.");

    Booking* stored = bookings_.insertBack(std::move(booking));
    if (stored == nullptr || !trackingIndex_.insert(stored->trackingId, stored)) {
        result.error = "The booking could not be indexed.";
        return result;
    }

    if (request.mode == BookingMode::QuickAssign) {
        if (assignNearestRider(*stored)) {
            const Rider* rider = findRider(stored->assignedRiderId);
            addStatus(
                *stored,
                DeliveryStatus::RiderAssigned,
                rider == nullptr ? "Nearest rider assigned." : rider->name + " accepted the automatic assignment."
            );
        }
    } else {
        createRiderOffers(*stored);
        if (!stored->offers.empty()) {
            addStatus(*stored, DeliveryStatus::OfferReceived, "Rider price offers are ready for customer selection.");
        }
    }

    saveRuntimeData();
    result.success = true;
    result.booking = stored;
    return result;
}

bool DeliverySystem::acceptOffer(
    const std::string& trackingId,
    const std::string& riderId,
    std::string& error
) {
    Booking* booking = findBooking(trackingId);
    if (booking == nullptr) {
        error = "Tracking ID was not found.";
        return false;
    }
    if (booking->mode != BookingMode::RiderOffers || booking->status != DeliveryStatus::OfferReceived) {
        error = "This booking is not waiting for an offer selection.";
        return false;
    }

    const RiderOffer* selectedOffer = nullptr;
    for (std::size_t index = 0; index < booking->offers.size(); ++index) {
        if (booking->offers[index].riderId == riderId) {
            selectedOffer = &booking->offers[index];
            break;
        }
    }
    if (selectedOffer == nullptr) {
        error = "The selected rider offer does not exist.";
        return false;
    }

    Rider* rider = findRider(riderId);
    if (rider == nullptr || !rider->available) {
        error = "The selected rider is no longer available.";
        return false;
    }

    rider->available = false;
    rider->activeTrackingId = trackingId;
    booking->assignedRiderId = riderId;
    booking->acceptedFare = selectedOffer->amount;
    addStatus(*booking, DeliveryStatus::RiderAssigned, rider->name + " offer accepted by customer.");
    saveRuntimeData();
    return true;
}

bool DeliverySystem::advanceBooking(const std::string& trackingId, std::string& error) {
    Booking* booking = findBooking(trackingId);
    if (booking == nullptr) {
        error = "Tracking ID was not found.";
        return false;
    }

    const DeliveryStatus target = nextStatus(booking->status);
    if (target == booking->status) {
        error = "This booking cannot move to another status.";
        return false;
    }

    switch (target) {
        case DeliveryStatus::RiderArriving:
            addStatus(*booking, target, "Rider is travelling to the pickup location.");
            break;
        case DeliveryStatus::PickedUp:
            addStatus(*booking, target, "Parcel collected from the customer.");
            break;
        case DeliveryStatus::InTransit:
            addStatus(*booking, target, "Parcel is moving along the selected delivery route.");
            break;
        case DeliveryStatus::Delivered: {
            addStatus(*booking, target, "Parcel delivered successfully.");
            Rider* rider = findRider(booking->assignedRiderId);
            if (rider != nullptr) {
                rider->available = true;
                rider->currentLocationId = booking->destinationId;
                rider->activeTrackingId.clear();
                ++rider->completedDeliveries;
                rider->earnings += booking->acceptedFare * 0.78;
            }
            CompletedRecord record;
            record.trackingId = booking->trackingId;
            record.customerName = booking->customerName;
            record.riderId = booking->assignedRiderId;
            record.fare = booking->acceptedFare;
            record.distanceKm = booking->deliveryRoute.distanceKm;
            completedRecords_.insert(record);
            break;
        }
        default:
            error = "The booking must have an assigned rider before delivery can continue.";
            return false;
    }
    saveRuntimeData();
    return true;
}

bool DeliverySystem::cancelBooking(const std::string& trackingId, std::string& error) {
    Booking* booking = findBooking(trackingId);
    if (booking == nullptr) {
        error = "Tracking ID was not found.";
        return false;
    }
    if (booking->status == DeliveryStatus::PickedUp ||
        booking->status == DeliveryStatus::InTransit ||
        booking->status == DeliveryStatus::Delivered ||
        booking->status == DeliveryStatus::Cancelled) {
        error = "The parcel can no longer be cancelled.";
        return false;
    }

    Rider* rider = findRider(booking->assignedRiderId);
    if (rider != nullptr) {
        rider->available = true;
        rider->activeTrackingId.clear();
    }
    addStatus(*booking, DeliveryStatus::Cancelled, "Booking cancelled before parcel pickup.");
    saveRuntimeData();
    return true;
}

bool DeliverySystem::rateDelivery(
    const std::string& trackingId,
    int rating,
    std::string& error
) {
    Booking* booking = findBooking(trackingId);
    if (booking == nullptr) {
        error = "Tracking ID was not found.";
        return false;
    }
    if (booking->status != DeliveryStatus::Delivered) {
        error = "A delivery can be rated only after it is completed.";
        return false;
    }
    if (booking->customerRating != 0) {
        error = "This delivery has already been rated.";
        return false;
    }
    if (rating < 1 || rating > 5) {
        error = "Choose a rating from 1 to 5.";
        return false;
    }
    Rider* rider = findRider(booking->assignedRiderId);
    if (rider == nullptr) {
        error = "The assigned rider profile was not found.";
        return false;
    }

    const double total = rider->rating * static_cast<double>(rider->ratingCount);
    ++rider->ratingCount;
    rider->rating = (total + static_cast<double>(rating)) /
        static_cast<double>(rider->ratingCount);
    booking->customerRating = rating;
    saveRuntimeData();
    return true;
}

Rider* DeliverySystem::registerRider(
    const RegisterRiderRequest& request,
    std::string& error
) {
    if (!ready_) {
        error = "The rider service is unavailable.";
        return nullptr;
    }
    if (!Validation::isPersonName(request.name)) {
        error = "Enter a valid rider name.";
        return nullptr;
    }
    if (!Validation::isPhoneNumber(request.phone)) {
        error = "Enter a valid rider phone number.";
        return nullptr;
    }
    if (!Validation::isVehicleName(request.vehicle)) {
        error = "Choose Bike, Car, Van, or Pickup as the vehicle type.";
        return nullptr;
    }
    if (!Validation::isVehicleRegistration(request.registration)) {
        error = "Enter a valid vehicle registration containing letters and numbers.";
        return nullptr;
    }
    if (!routeEngine_->graph().validVertex(request.locationId)) {
        error = "Choose a valid starting location.";
        return nullptr;
    }
    if (!Validation::isMoney(request.baseCharge, 2000.0) ||
        !Validation::isMoney(request.perKmCharge, 500.0) ||
        request.perKmCharge < 1.0) {
        error = "Rider charges are outside the allowed range.";
        return nullptr;
    }
    for (std::size_t index = 0; index < riders_.size(); ++index) {
        if (Validation::equalsIgnoreCase(riders_[index].registration, request.registration)) {
            error = "This vehicle registration is already registered.";
            return nullptr;
        }
    }

    Rider rider;
    ++riderSequence_;
    rider.id = "RD-" + std::to_string(riderSequence_);
    rider.name = Validation::collapseSpaces(Validation::trim(request.name));
    rider.phone = Validation::collapseSpaces(Validation::trim(request.phone));
    rider.currentLocationId = request.locationId;
    // A newly registered rider has no customer feedback yet. Ratings are
    // never fabricated at registration time.
    rider.rating = 0.0;
    rider.vehicle = request.vehicle;
    rider.registration = Validation::collapseSpaces(Validation::trim(request.registration));
    rider.baseCharge = request.baseCharge;
    rider.perKmCharge = request.perKmCharge;
    rider.available = true;
    riders_.pushBack(std::move(rider));
    saveRuntimeData();
    return &riders_.back();
}

bool DeliverySystem::updateRider(
    const std::string& riderId,
    bool available,
    int locationId,
    double baseCharge,
    double perKmCharge,
    std::string& error
) {
    Rider* rider = findRider(riderId);
    if (rider == nullptr) {
        error = "Rider ID was not found.";
        return false;
    }
    if (!rider->activeTrackingId.empty() && available) {
        error = "A rider with an active delivery cannot be marked available manually.";
        return false;
    }
    if (!routeEngine_->graph().validVertex(locationId)) {
        error = "Choose a valid current location.";
        return false;
    }
    if (!rider->activeTrackingId.empty() && locationId != rider->currentLocationId) {
        error = "Current location cannot be changed during an active delivery.";
        return false;
    }
    if (!Validation::isMoney(baseCharge, 2000.0) ||
        !Validation::isMoney(perKmCharge, 500.0) || perKmCharge < 1.0) {
        error = "Rider charges are outside the allowed range.";
        return false;
    }
    rider->available = available;
    rider->currentLocationId = locationId;
    rider->baseCharge = baseCharge;
    rider->perKmCharge = perKmCharge;
    saveRuntimeData();
    return true;
}

int DeliverySystem::advanceDueBookings() {
    const long long now = currentEpochSeconds();
    DynamicArray<std::string> dueTrackingIds;
    bookings_.forEach([now, &dueTrackingIds](const Booking& booking) {
        if (booking.nextStatusAtEpoch > 0 && now >= booking.nextStatusAtEpoch) {
            dueTrackingIds.pushBack(booking.trackingId);
        }
    });

    int advanced = 0;
    for (std::size_t index = 0; index < dueTrackingIds.size(); ++index) {
        std::string ignoredError;
        if (advanceBooking(dueTrackingIds[index], ignoredError)) ++advanced;
    }
    return advanced;
}

void DeliverySystem::recalculateActiveRoutes() {
    if (!ready_) return;
    bookings_.forEach([this](Booking& booking) {
        if (booking.status == DeliveryStatus::Delivered ||
            booking.status == DeliveryStatus::Cancelled) {
            return;
        }
        RouteResult updated = routeEngine_->minimumDistance(booking.pickupId, booking.destinationId);
        if (updated.found) {
            booking.deliveryRoute = std::move(updated);
        }
    });
}

Booking* DeliverySystem::findBooking(const std::string& trackingId) {
    Booking** stored = trackingIndex_.find(trackingId);
    return stored == nullptr ? nullptr : *stored;
}

const Booking* DeliverySystem::findBooking(const std::string& trackingId) const {
    Booking* const* stored = trackingIndex_.find(trackingId);
    return stored == nullptr ? nullptr : *stored;
}

Rider* DeliverySystem::findRider(const std::string& riderId) {
    const int index = binarySearchIndex(
        riders_,
        riderId,
        [](const Rider& rider) -> const std::string& { return rider.id; }
    );
    return index < 0 ? nullptr : &riders_[static_cast<std::size_t>(index)];
}

const Rider* DeliverySystem::findRider(const std::string& riderId) const {
    const int index = binarySearchIndex(
        riders_,
        riderId,
        [](const Rider& rider) -> const std::string& { return rider.id; }
    );
    return index < 0 ? nullptr : &riders_[static_cast<std::size_t>(index)];
}

const DynamicArray<Rider>& DeliverySystem::riders() const { return riders_; }
const SinglyLinkedList<Booking>& DeliverySystem::bookings() const { return bookings_; }
const AVLTree<CompletedRecord>& DeliverySystem::completedRecords() const { return completedRecords_; }

DashboardSummary DeliverySystem::dashboardSummary() const {
    DashboardSummary summary;
    summary.totalBookings = static_cast<int>(bookings_.size());

    bookings_.forEach([&summary](const Booking& booking) {
        switch (booking.status) {
            case DeliveryStatus::Delivered:
                ++summary.deliveredBookings;
                summary.totalRevenue += booking.acceptedFare;
                summary.riderPayouts += booking.acceptedFare * 0.78;
                summary.platformProfit += booking.acceptedFare * 0.22;
                break;
            case DeliveryStatus::Cancelled:
                ++summary.cancelledBookings;
                break;
            case DeliveryStatus::Requested:
            case DeliveryStatus::SearchingForRider:
            case DeliveryStatus::OfferReceived:
                ++summary.pendingBookings;
                break;
            default:
                ++summary.activeDeliveries;
                break;
        }
    });

    for (std::size_t index = 0; index < riders_.size(); ++index) {
        if (riders_[index].available) ++summary.availableRiders;
    }
    return summary;
}

const char* DeliverySystem::priorityName(ParcelPriority priority) {
    switch (priority) {
        case ParcelPriority::Express: return "Express";
        case ParcelPriority::SameDay: return "Same Day";
        default: return "Standard";
    }
}

const char* DeliverySystem::modeName(BookingMode mode) {
    return mode == BookingMode::RiderOffers ? "Rider Offers" : "Quick Assign";
}

const char* DeliverySystem::statusName(DeliveryStatus status) {
    switch (status) {
        case DeliveryStatus::Requested: return "Requested";
        case DeliveryStatus::SearchingForRider: return "Searching for Rider";
        case DeliveryStatus::OfferReceived: return "Offer Received";
        case DeliveryStatus::RiderAssigned: return "Rider Assigned";
        case DeliveryStatus::RiderArriving: return "Rider Arriving";
        case DeliveryStatus::PickedUp: return "Picked Up";
        case DeliveryStatus::InTransit: return "In Transit";
        case DeliveryStatus::Delivered: return "Delivered";
        case DeliveryStatus::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

ParcelPriority DeliverySystem::parsePriority(const std::string& value) {
    if (value == "Express") return ParcelPriority::Express;
    if (value == "Same Day" || value == "SameDay") return ParcelPriority::SameDay;
    return ParcelPriority::Standard;
}

BookingMode DeliverySystem::parseMode(const std::string& value) {
    return value == "offers" || value == "Rider Offers"
        ? BookingMode::RiderOffers
        : BookingMode::QuickAssign;
}

bool DeliverySystem::loadRiders(const std::filesystem::path& ridersFile) {
    std::ifstream input(ridersFile);
    if (!input) return false;

    riders_.clear();
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        const CsvRow fields = splitCsvLine(line);
        if (fields.count != 8 && fields.count != 11) return false;

        try {
            Rider rider;
            rider.id = fields[0];
            rider.name = fields[1];
            if (fields.count == 11) {
                rider.phone = fields[2];
                rider.currentLocationId = std::stoi(fields[3]);
                rider.rating = std::stod(fields[4]);
                rider.vehicle = fields[5];
                rider.registration = fields[6];
                rider.baseCharge = std::stod(fields[7]);
                rider.perKmCharge = std::stod(fields[8]);
                rider.available = fields[9] == "1";
                rider.completedDeliveries = std::stoi(fields[10]);
            } else {
                rider.phone = "0300-0000000";
                rider.currentLocationId = std::stoi(fields[2]);
                rider.rating = std::stod(fields[3]);
                rider.vehicle = fields[4];
                rider.registration = fields[5];
                rider.available = fields[6] == "1";
                rider.completedDeliveries = std::stoi(fields[7]);
            }
            const std::size_t separator = rider.id.find_last_of('-');
            if (separator != std::string::npos) {
                const unsigned long sequence = std::stoul(rider.id.substr(separator + 1));
                if (sequence > riderSequence_) riderSequence_ = sequence;
            }
            riders_.pushBack(std::move(rider));
        } catch (...) {
            return false;
        }
    }
    for (std::size_t index = 1; index < riders_.size(); ++index) {
        if (!(riders_[index - 1].id < riders_[index].id)) return false;
    }
    // A clean installation intentionally starts with no rider profiles.
    // Registered riders are restored from runtime persistence.
    return true;
}

void DeliverySystem::loadRuntimeData() {
    loadRiderState();
    loadBookings();
    loadHistory();

    bookings_.forEach([this](Booking& booking) {
        if (booking.history.empty()) {
            addStatus(booking, booking.status, "Booking restored from persistent storage.");
        }
        if (booking.status == DeliveryStatus::OfferReceived && booking.offers.empty()) {
            createRiderOffers(booking);
        }
        if (booking.nextStatusAtEpoch == 0 &&
            (booking.status == DeliveryStatus::RiderAssigned ||
             booking.status == DeliveryStatus::RiderArriving ||
             booking.status == DeliveryStatus::PickedUp ||
             booking.status == DeliveryStatus::InTransit)) {
            scheduleAutomaticUpdate(booking);
        }
    });
}

void DeliverySystem::loadRiderState() {
    std::ifstream input(runtimeDirectory_ / "riders.db");
    if (!input) return;

    std::string line;
    while (std::getline(input, line)) {
        const CsvRow fields = splitLine(line, '\t');
        if (fields.count != 6 && fields.count != 13 && fields.count != 14) continue;
        try {
            Rider* rider = findRider(fields[0]);
            if (fields.count == 13 || fields.count == 14) {
                Rider restored;
                restored.id = fields[0];
                restored.name = fields[1];
                restored.phone = fields[2];
                restored.currentLocationId = std::stoi(fields[3]);
                restored.rating = std::stod(fields[4]);
                restored.vehicle = fields[5];
                restored.registration = fields[6];
                restored.available = fields[7] == "1";
                restored.completedDeliveries = std::stoi(fields[8]);
                restored.earnings = std::stod(fields[9]);
                restored.activeTrackingId = fields[10];
                restored.baseCharge = std::stod(fields[11]);
                restored.perKmCharge = std::stod(fields[12]);
                if (fields.count == 14) restored.ratingCount = std::stoi(fields[13]);
                if (rider == nullptr) {
                    riders_.pushBack(std::move(restored));
                    rider = &riders_.back();
                } else {
                    *rider = std::move(restored);
                }
            } else {
                if (rider == nullptr) continue;
                rider->currentLocationId = std::stoi(fields[1]);
                rider->available = fields[2] == "1";
                rider->completedDeliveries = std::stoi(fields[3]);
                rider->earnings = std::stod(fields[4]);
                rider->activeTrackingId = fields[5];
            }
            const std::size_t separator = fields[0].find_last_of('-');
            if (separator != std::string::npos) {
                const unsigned long sequence = std::stoul(fields[0].substr(separator + 1));
                if (sequence > riderSequence_) riderSequence_ = sequence;
            }
        } catch (...) {
            // Keep the seed values if a saved row is malformed.
        }
    }
}

void DeliverySystem::loadBookings() {
    std::ifstream input(runtimeDirectory_ / "bookings.db");
    if (!input) return;

    std::string line;
    while (std::getline(input, line)) {
        const CsvRow fields = splitLine(line, '\t');
        if ((fields.count != 14 && fields.count != 16 && fields.count != 17) ||
            findBooking(fields[0]) != nullptr) continue;

        try {
            Booking booking;
            booking.trackingId = fields[0];
            booking.customerName = fields[1];
            booking.receiverName = fields[2];
            booking.receiverPhone = fields[3];
            booking.pickupId = std::stoi(fields[4]);
            booking.destinationId = std::stoi(fields[5]);
            booking.weightKg = std::stod(fields[6]);
            booking.priority = static_cast<ParcelPriority>(std::stoi(fields[7]));
            booking.mode = static_cast<BookingMode>(std::stoi(fields[8]));
            booking.status = static_cast<DeliveryStatus>(std::stoi(fields[9]));
            booking.estimatedFare = std::stod(fields[10]);
            booking.acceptedFare = std::stod(fields[11]);
            booking.assignedRiderId = fields[12];
            booking.createdAt = fields[13];
            if (fields.count >= 16) {
                booking.statusChangedAtEpoch = std::stoll(fields[14]);
                booking.nextStatusAtEpoch = std::stoll(fields[15]);
            }
            if (fields.count == 17) booking.customerRating = std::stoi(fields[16]);
            booking.deliveryRoute = routeEngine_->minimumDistance(
                booking.pickupId,
                booking.destinationId
            );

            const std::size_t separator = booking.trackingId.find_last_of('-');
            if (separator != std::string::npos) {
                const unsigned long sequence = std::stoul(booking.trackingId.substr(separator + 1));
                if (sequence > bookingSequence_) bookingSequence_ = sequence;
            }

            const bool delivered = booking.status == DeliveryStatus::Delivered;
            CompletedRecord record;
            if (delivered) {
                record.trackingId = booking.trackingId;
                record.customerName = booking.customerName;
                record.riderId = booking.assignedRiderId;
                record.fare = booking.acceptedFare;
                record.distanceKm = booking.deliveryRoute.distanceKm;
            }

            Booking* stored = bookings_.insertBack(std::move(booking));
            if (stored != nullptr) {
                trackingIndex_.insert(stored->trackingId, stored);
                if (delivered) completedRecords_.insert(record);
            }
        } catch (...) {
            // Ignore malformed saved rows while keeping all valid records available.
        }
    }
}

void DeliverySystem::loadHistory() {
    std::ifstream input(runtimeDirectory_ / "history.db");
    if (!input) return;

    std::string line;
    while (std::getline(input, line)) {
        const CsvRow fields = splitLine(line, '\t');
        if (fields.count != 4) continue;

        Booking* booking = findBooking(fields[0]);
        if (booking == nullptr) continue;
        try {
            StatusEvent event;
            event.status = static_cast<DeliveryStatus>(std::stoi(fields[1]));
            event.time = fields[2];
            event.note = fields[3];
            booking->history.pushBack(event);
        } catch (...) {
            // Ignore malformed history entries.
        }
    }
}

void DeliverySystem::saveRuntimeData() const {
    if (!persistenceEnabled_) return;

    std::error_code error;
    std::filesystem::create_directories(runtimeDirectory_, error);
    if (error) return;

    std::ofstream bookingsFile(runtimeDirectory_ / "bookings.db", std::ios::trunc);
    std::ofstream historyFile(runtimeDirectory_ / "history.db", std::ios::trunc);
    std::ofstream ridersFile(runtimeDirectory_ / "riders.db", std::ios::trunc);
    if (!bookingsFile || !historyFile || !ridersFile) return;

    bookingsFile << std::setprecision(12);
    bookings_.forEach([&bookingsFile, &historyFile](const Booking& booking) {
        bookingsFile
            << safeField(booking.trackingId) << '\t'
            << safeField(booking.customerName) << '\t'
            << safeField(booking.receiverName) << '\t'
            << safeField(booking.receiverPhone) << '\t'
            << booking.pickupId << '\t'
            << booking.destinationId << '\t'
            << booking.weightKg << '\t'
            << static_cast<int>(booking.priority) << '\t'
            << static_cast<int>(booking.mode) << '\t'
            << static_cast<int>(booking.status) << '\t'
            << booking.estimatedFare << '\t'
            << booking.acceptedFare << '\t'
            << safeField(booking.assignedRiderId) << '\t'
            << safeField(booking.createdAt) << '\t'
            << booking.statusChangedAtEpoch << '\t'
            << booking.nextStatusAtEpoch << '\t'
            << booking.customerRating << '\n';

        booking.history.forEachForward([&historyFile, &booking](const StatusEvent& event) {
            historyFile
                << safeField(booking.trackingId) << '\t'
                << static_cast<int>(event.status) << '\t'
                << safeField(event.time) << '\t'
                << safeField(event.note) << '\n';
        });
    });

    ridersFile << std::setprecision(12);
    for (std::size_t index = 0; index < riders_.size(); ++index) {
        const Rider& rider = riders_[index];
        ridersFile
            << safeField(rider.id) << '\t'
            << safeField(rider.name) << '\t'
            << safeField(rider.phone) << '\t'
            << rider.currentLocationId << '\t'
            << rider.rating << '\t'
            << safeField(rider.vehicle) << '\t'
            << safeField(rider.registration) << '\t'
            << (rider.available ? 1 : 0) << '\t'
            << rider.completedDeliveries << '\t'
            << rider.earnings << '\t'
            << safeField(rider.activeTrackingId) << '\t'
            << rider.baseCharge << '\t'
            << rider.perKmCharge << '\t'
            << rider.ratingCount << '\n';
    }
}

std::string DeliverySystem::safeField(const std::string& value) {
    std::string safe = value;
    for (char& character : safe) {
        if (character == '\t' || character == '\r' || character == '\n') {
            character = ' ';
        }
    }
    return safe;
}

bool DeliverySystem::validateRequest(const CreateBookingRequest& request, std::string& error) const {
    if (!Validation::isPersonName(request.customerName) ||
        !Validation::isPersonName(request.receiverName)) {
        error = "Enter valid customer and receiver names.";
        return false;
    }
    if (!Validation::isPhoneNumber(request.receiverPhone)) {
        error = "Enter a valid receiver phone number.";
        return false;
    }
    if (!routeEngine_->graph().validVertex(request.pickupId) ||
        !routeEngine_->graph().validVertex(request.destinationId) ||
        request.pickupId == request.destinationId) {
        error = "Pickup and destination must be different valid locations.";
        return false;
    }
    if (!std::isfinite(request.weightKg) || request.weightKg < 0.1 || request.weightKg > 30.0) {
        error = "Parcel weight must be between 0.1 and 30 kilograms.";
        return false;
    }
    return true;
}

std::string DeliverySystem::nextTrackingId() {
    ++bookingSequence_;
    const auto now = std::chrono::system_clock::now();
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &rawTime);
#else
    localtime_r(&rawTime, &localTime);
#endif

    std::ostringstream id;
    id << "NX-" << std::put_time(&localTime, "%Y%m%d") << '-'
       << std::setw(4) << std::setfill('0') << bookingSequence_;
    return id.str();
}

double DeliverySystem::calculateFare(
    const CreateBookingRequest& request,
    const RouteResult& route
) const {
    double fare = 120.0 + route.distanceKm * 35.0;
    if (request.weightKg > 1.0) fare += (request.weightKg - 1.0) * 20.0;
    if (request.priority == ParcelPriority::SameDay) fare += 80.0;
    if (request.priority == ParcelPriority::Express) fare += 160.0;
    fare *= highestTrafficSurcharge(route);
    return roundToFive(fare);
}

void DeliverySystem::addStatus(
    Booking& booking,
    DeliveryStatus status,
    const std::string& note
) {
    booking.status = status;
    StatusEvent event;
    event.status = status;
    event.time = currentTimeText();
    event.note = note;
    booking.history.pushBack(event);
    scheduleAutomaticUpdate(booking);
}

void DeliverySystem::scheduleAutomaticUpdate(Booking& booking) {
    booking.statusChangedAtEpoch = currentEpochSeconds();
    int delaySeconds = 0;
    switch (booking.status) {
        case DeliveryStatus::RiderAssigned: delaySeconds = 2; break;
        case DeliveryStatus::RiderArriving: delaySeconds = 7; break;
        case DeliveryStatus::PickedUp: delaySeconds = 2; break;
        case DeliveryStatus::InTransit: delaySeconds = 10; break;
        default: break;
    }
    booking.nextStatusAtEpoch = delaySeconds == 0
        ? 0
        : booking.statusChangedAtEpoch + delaySeconds;
}

bool DeliverySystem::assignNearestRider(Booking& booking) {
    MinHeap<RiderCandidate> candidates;

    for (std::size_t index = 0; index < riders_.size(); ++index) {
        if (!riders_[index].available) continue;
        const RouteResult pickupRoute = routeEngine_->minimumDistance(
            riders_[index].currentLocationId,
            booking.pickupId
        );
        if (!pickupRoute.found) continue;

        RiderCandidate candidate;
        candidate.riderIndex = static_cast<int>(index);
        candidate.distanceKm = pickupRoute.distanceKm;
        candidate.pickupMinutes = pickupRoute.estimatedMinutes;
        candidate.rating = riders_[index].rating;
        candidates.insert(candidate);
    }

    RiderCandidate selected;
    if (!candidates.removeMinimum(selected)) return false;

    Rider& rider = riders_[static_cast<std::size_t>(selected.riderIndex)];
    rider.available = false;
    rider.activeTrackingId = booking.trackingId;
    booking.assignedRiderId = rider.id;
    return true;
}

void DeliverySystem::createRiderOffers(Booking& booking) {
    CircularQueue<int> rotation(riders_.size());
    for (std::size_t index = 0; index < riders_.size(); ++index) {
        if (riders_[index].available) {
            rotation.enqueue(static_cast<int>(index));
        }
    }

    static const double offerFactors[4] = {0.93, 0.98, 1.03, 1.08};
    int riderIndex = -1;
    int offerIndex = 0;
    while (offerIndex < 4 && rotation.dequeue(riderIndex)) {
        Rider& rider = riders_[static_cast<std::size_t>(riderIndex)];
        const RouteResult pickupRoute = routeEngine_->minimumDistance(
            rider.currentLocationId,
            booking.pickupId
        );
        if (!pickupRoute.found) continue;

        RiderOffer offer;
        offer.riderId = rider.id;
        offer.riderName = rider.name;
        offer.vehicle = rider.vehicle;
        offer.rating = rider.rating;
        offer.pickupDistanceKm = pickupRoute.distanceKm;
        offer.pickupMinutes = pickupRoute.estimatedMinutes;
        const double riderPrice = rider.baseCharge +
            booking.deliveryRoute.distanceKm * rider.perKmCharge +
            pickupRoute.distanceKm * 8.0;
        offer.amount = roundToFive(
            (riderPrice * 0.55 + booking.estimatedFare * 0.45) * offerFactors[offerIndex]
        );
        booking.offers.pushBack(offer);
        ++offerIndex;
    }
}

double DeliverySystem::highestTrafficSurcharge(const RouteResult& route) const {
    double factor = 1.0;
    for (std::size_t index = 0; index + 1 < route.path.size(); ++index) {
        const EdgeNode* edge = routeEngine_->graph().edgeBetween(route.path[index], route.path[index + 1]);
        if (edge == nullptr) continue;
        if (edge->traffic == TrafficLevel::High) return 1.12;
        if (edge->traffic == TrafficLevel::Medium) factor = 1.05;
    }
    return factor;
}

DeliveryStatus DeliverySystem::nextStatus(DeliveryStatus current) const {
    switch (current) {
        case DeliveryStatus::RiderAssigned: return DeliveryStatus::RiderArriving;
        case DeliveryStatus::RiderArriving: return DeliveryStatus::PickedUp;
        case DeliveryStatus::PickedUp: return DeliveryStatus::InTransit;
        case DeliveryStatus::InTransit: return DeliveryStatus::Delivered;
        default: return current;
    }
}

std::string DeliverySystem::currentTimeText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &rawTime);
#else
    localtime_r(&rawTime, &localTime);
#endif

    std::ostringstream value;
    value << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return value.str();
}

long long DeliverySystem::currentEpochSeconds() {
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count());
}
