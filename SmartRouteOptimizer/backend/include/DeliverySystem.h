#pragma once

#include "AVLTree.h"
#include "DoublyLinkedList.h"
#include "DynamicArray.h"
#include "HashTable.h"
#include "RouteEngine.h"
#include "SinglyLinkedList.h"

#include <filesystem>
#include <string>

enum class ParcelPriority {
    Express = 1,
    SameDay = 2,
    Standard = 3
};

enum class BookingMode {
    QuickAssign,
    RiderOffers
};

enum class DeliveryStatus {
    Requested,
    SearchingForRider,
    OfferReceived,
    RiderAssigned,
    RiderArriving,
    PickedUp,
    InTransit,
    Delivered,
    Cancelled
};

struct Rider {
    std::string id;
    std::string name;
    std::string phone;
    int currentLocationId = 0;
    double rating = 0.0;
    int ratingCount = 0;
    std::string vehicle;
    std::string registration;
    double baseCharge = 120.0;
    double perKmCharge = 28.0;
    bool available = true;
    int completedDeliveries = 0;
    double earnings = 0.0;
    std::string activeTrackingId;
};

struct RiderOffer {
    std::string riderId;
    std::string riderName;
    std::string vehicle;
    double rating = 0.0;
    double pickupDistanceKm = 0.0;
    double pickupMinutes = 0.0;
    double amount = 0.0;
};

struct StatusEvent {
    DeliveryStatus status = DeliveryStatus::Requested;
    std::string time;
    std::string note;
};

struct Booking {
    std::string trackingId;
    std::string customerName;
    std::string receiverName;
    std::string receiverPhone;
    int pickupId = -1;
    int destinationId = -1;
    double weightKg = 0.0;
    ParcelPriority priority = ParcelPriority::Standard;
    BookingMode mode = BookingMode::QuickAssign;
    DeliveryStatus status = DeliveryStatus::Requested;
    double estimatedFare = 0.0;
    double acceptedFare = 0.0;
    std::string assignedRiderId;
    RouteResult deliveryRoute;
    DynamicArray<RiderOffer> offers;
    DoublyLinkedList<StatusEvent> history;
    std::string createdAt;
    long long statusChangedAtEpoch = 0;
    long long nextStatusAtEpoch = 0;
    int customerRating = 0;

    Booking() = default;
    Booking(const Booking&) = delete;
    Booking& operator=(const Booking&) = delete;
    Booking(Booking&&) noexcept = default;
    Booking& operator=(Booking&&) noexcept = default;
};

struct CompletedRecord {
    std::string trackingId;
    std::string customerName;
    std::string riderId;
    double fare = 0.0;
    double distanceKm = 0.0;

    bool operator<(const CompletedRecord& other) const {
        return trackingId < other.trackingId;
    }
};

struct CreateBookingRequest {
    std::string customerName;
    std::string receiverName;
    std::string receiverPhone;
    int pickupId = -1;
    int destinationId = -1;
    double weightKg = 0.0;
    ParcelPriority priority = ParcelPriority::Standard;
    BookingMode mode = BookingMode::QuickAssign;
};

struct RegisterRiderRequest {
    std::string name;
    std::string phone;
    int locationId = -1;
    std::string vehicle;
    std::string registration;
    double baseCharge = 120.0;
    double perKmCharge = 28.0;
};

struct BookingCreationResult {
    bool success = false;
    std::string error;
    Booking* booking = nullptr;
};

struct DashboardSummary {
    int totalBookings = 0;
    int activeDeliveries = 0;
    int pendingBookings = 0;
    int deliveredBookings = 0;
    int cancelledBookings = 0;
    int availableRiders = 0;
    double totalRevenue = 0.0;
    double riderPayouts = 0.0;
    double platformProfit = 0.0;
};

class DeliverySystem {
public:
    DeliverySystem();

    bool initialize(
        RouteEngine* routeEngine,
        const std::filesystem::path& ridersFile,
        const std::filesystem::path& runtimeDirectory = {}
    );
    bool ready() const;

    BookingCreationResult createBooking(const CreateBookingRequest& request);
    bool acceptOffer(const std::string& trackingId, const std::string& riderId, std::string& error);
    bool advanceBooking(const std::string& trackingId, std::string& error);
    bool cancelBooking(const std::string& trackingId, std::string& error);
    bool rateDelivery(const std::string& trackingId, int rating, std::string& error);
    Rider* registerRider(const RegisterRiderRequest& request, std::string& error);
    bool updateRider(
        const std::string& riderId,
        bool available,
        int locationId,
        double baseCharge,
        double perKmCharge,
        std::string& error
    );
    int advanceDueBookings();
    void recalculateActiveRoutes();

    Booking* findBooking(const std::string& trackingId);
    const Booking* findBooking(const std::string& trackingId) const;
    Rider* findRider(const std::string& riderId);
    const Rider* findRider(const std::string& riderId) const;

    const DynamicArray<Rider>& riders() const;
    const SinglyLinkedList<Booking>& bookings() const;
    const AVLTree<CompletedRecord>& completedRecords() const;
    DashboardSummary dashboardSummary() const;

    static const char* priorityName(ParcelPriority priority);
    static const char* modeName(BookingMode mode);
    static const char* statusName(DeliveryStatus status);
    static ParcelPriority parsePriority(const std::string& value);
    static BookingMode parseMode(const std::string& value);

private:
    RouteEngine* routeEngine_;
    bool ready_;
    DynamicArray<Rider> riders_;
    SinglyLinkedList<Booking> bookings_;
    HashTable<Booking*> trackingIndex_;
    AVLTree<CompletedRecord> completedRecords_;
    unsigned long bookingSequence_;
    unsigned long riderSequence_;
    std::filesystem::path runtimeDirectory_;
    bool persistenceEnabled_;

    bool loadRiders(const std::filesystem::path& ridersFile);
    void loadRuntimeData();
    void loadRiderState();
    void loadBookings();
    void loadHistory();
    void saveRuntimeData() const;
    static std::string safeField(const std::string& value);
    bool validateRequest(const CreateBookingRequest& request, std::string& error) const;
    std::string nextTrackingId();
    double calculateFare(const CreateBookingRequest& request, const RouteResult& route) const;
    void addStatus(Booking& booking, DeliveryStatus status, const std::string& note);
    void scheduleAutomaticUpdate(Booking& booking);
    bool assignNearestRider(Booking& booking);
    void createRiderOffers(Booking& booking);
    double highestTrafficSurcharge(const RouteResult& route) const;
    DeliveryStatus nextStatus(DeliveryStatus current) const;
    static std::string currentTimeText();
    static long long currentEpochSeconds();
};
