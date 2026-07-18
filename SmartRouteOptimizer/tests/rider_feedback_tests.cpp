#include "DeliverySystem.h"
#include "RouteEngine.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

namespace {
bool nearlyEqual(double left, double right) {
    return std::fabs(left - right) < 0.001;
}

RegisterRiderRequest riderRequestAt(int locationId) {
    RegisterRiderRequest request;
    request.name = "Dynamic Test Rider";
    request.phone = "03005550777";
    request.locationId = locationId;
    request.vehicle = "Bike";
    request.registration = "DYN-777";
    request.baseCharge = 135.0;
    request.perKmCharge = 30.0;
    return request;
}

CreateBookingRequest bookingRequest(
    int pickupId,
    int destinationId,
    const std::string& receiverName
) {
    CreateBookingRequest request;
    request.customerName = "Dynamic Test Customer";
    request.receiverName = receiverName;
    request.receiverPhone = "03001234567";
    request.pickupId = pickupId;
    request.destinationId = destinationId;
    request.weightKg = 1.5;
    request.priority = ParcelPriority::Standard;
    request.mode = BookingMode::QuickAssign;
    return request;
}

void completeDelivery(
    DeliverySystem& system,
    Booking& booking,
    std::string& error
) {
    assert(booking.status == DeliveryStatus::RiderAssigned);
    assert(system.advanceBooking(booking.trackingId, error));
    assert(booking.status == DeliveryStatus::RiderArriving);
    assert(system.advanceBooking(booking.trackingId, error));
    assert(booking.status == DeliveryStatus::PickedUp);
    assert(system.advanceBooking(booking.trackingId, error));
    assert(booking.status == DeliveryStatus::InTransit);
    assert(system.advanceBooking(booking.trackingId, error));
    assert(booking.status == DeliveryStatus::Delivered);
}

void testCleanRegistrationLocationAndFeedback() {
    RouteEngine routes;
    assert(routes.load("data/locations.csv", "data/roads.csv"));

    DeliverySystem system;
    assert(system.initialize(&routes, "data/riders.csv"));
    assert(system.ready());
    assert(system.riders().empty());
    assert(system.bookings().empty());
    assert(system.completedRecords().size() == 0);

    const DashboardSummary clean = system.dashboardSummary();
    assert(clean.totalBookings == 0);
    assert(clean.availableRiders == 0);
    assert(clean.deliveredBookings == 0);
    assert(nearlyEqual(clean.totalRevenue, 0.0));

    std::string error;
    RegisterRiderRequest invalid = riderRequestAt(999);
    assert(system.registerRider(invalid, error) == nullptr);
    assert(error == "Choose a valid starting location.");

    Rider* rider = system.registerRider(riderRequestAt(5), error);
    assert(rider != nullptr);
    assert(rider->id == "RD-101");
    assert(rider->currentLocationId == 5);
    assert(rider->ratingCount == 0);
    assert(nearlyEqual(rider->rating, 0.0));
    assert(rider->completedDeliveries == 0);
    assert(nearlyEqual(rider->earnings, 0.0));

    assert(!system.updateRider(rider->id, true, -1, 140.0, 31.0, error));
    assert(system.updateRider(rider->id, true, 6, 140.0, 31.0, error));
    assert(rider->currentLocationId == 6);
    assert(nearlyEqual(rider->baseCharge, 140.0));
    assert(nearlyEqual(rider->perKmCharge, 31.0));

    BookingCreationResult first = system.createBooking(
        bookingRequest(5, 7, "First Receiver")
    );
    assert(first.success);
    assert(first.booking != nullptr);
    assert(first.booking->assignedRiderId == rider->id);
    assert(!rider->available);
    assert(!system.updateRider(rider->id, false, 8, 150.0, 32.0, error));
    assert(error == "Current location cannot be changed during an active delivery.");
    assert(!system.rateDelivery(first.booking->trackingId, 5, error));
    assert(error == "A delivery can be rated only after it is completed.");

    completeDelivery(system, *first.booking, error);
    assert(rider->currentLocationId == 7);
    assert(!system.rateDelivery(first.booking->trackingId, 0, error));
    assert(!system.rateDelivery(first.booking->trackingId, 6, error));
    assert(system.rateDelivery(first.booking->trackingId, 5, error));
    assert(first.booking->customerRating == 5);
    assert(rider->ratingCount == 1);
    assert(nearlyEqual(rider->rating, 5.0));
    assert(!system.rateDelivery(first.booking->trackingId, 4, error));
    assert(error == "This delivery has already been rated.");

    BookingCreationResult second = system.createBooking(
        bookingRequest(7, 0, "Second Receiver")
    );
    assert(second.success);
    assert(second.booking != nullptr);
    assert(second.booking->assignedRiderId == rider->id);
    completeDelivery(system, *second.booking, error);
    assert(system.rateDelivery(second.booking->trackingId, 3, error));
    assert(second.booking->customerRating == 3);
    assert(rider->ratingCount == 2);
    assert(nearlyEqual(rider->rating, 4.0));
    assert(rider->completedDeliveries == 2);
    assert(rider->earnings > 0.0);

    const DashboardSummary used = system.dashboardSummary();
    assert(used.totalBookings == 2);
    assert(used.deliveredBookings == 2);
    assert(used.availableRiders == 1);
    assert(used.totalRevenue > 0.0);
    assert(system.completedRecords().size() == 2);
}
}

int main() {
    testCleanRegistrationLocationAndFeedback();
    std::cout
        << "Clean rider state, required starting location, editable current "
        << "location, and genuine customer feedback tests passed.\n";
    return 0;
}
