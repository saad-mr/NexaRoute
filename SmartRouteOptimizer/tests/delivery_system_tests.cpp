#include "AVLTree.h"
#include "BinarySearch.h"
#include "CircularQueue.h"
#include "DeliverySystem.h"
#include "DoublyLinkedList.h"
#include "HashTable.h"
#include "MinHeap.h"
#include "SinglyLinkedList.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
struct HeapValue {
    int priority = 0;
    bool operator<(const HeapValue& other) const { return priority < other.priority; }
};

struct TreeValue {
    int key = 0;
    bool operator<(const TreeValue& other) const { return key < other.key; }
};

void testAdditionalStructures() {
    DynamicArray<int> sortedValues;
    sortedValues.pushBack(4);
    sortedValues.pushBack(9);
    sortedValues.pushBack(15);
    sortedValues.pushBack(22);
    const auto identity = [](int value) { return value; };
    assert(binarySearchIndex(sortedValues, 15, identity) == 2);
    assert(binarySearchIndex(sortedValues, 7, identity) == -1);

    SinglyLinkedList<int> singly;
    singly.insertBack(10);
    singly.insertBack(20);
    singly.insertBack(30);
    assert(singly.size() == 3);
    int sum = 0;
    singly.forEach([&sum](int value) { sum += value; });
    assert(sum == 60);

    DoublyLinkedList<std::string> doubly;
    doubly.pushBack("Requested");
    doubly.pushBack("Assigned");
    doubly.pushBack("Delivered");
    std::string reversed;
    doubly.forEachBackward([&reversed](const std::string& value) {
        if (!reversed.empty()) reversed += ',';
        reversed += value;
    });
    assert(reversed == "Delivered,Assigned,Requested");

    CircularQueue<int> circular(3);
    assert(circular.enqueue(1));
    assert(circular.enqueue(2));
    assert(circular.enqueue(3));
    assert(!circular.enqueue(4));
    int value = 0;
    assert(circular.dequeue(value) && value == 1);
    assert(circular.enqueue(4));
    assert(circular.dequeue(value) && value == 2);
    assert(circular.dequeue(value) && value == 3);
    assert(circular.dequeue(value) && value == 4);

    MinHeap<HeapValue> heap;
    heap.insert({3});
    heap.insert({1});
    heap.insert({5});
    heap.insert({2});
    HeapValue minimum;
    assert(heap.removeMinimum(minimum) && minimum.priority == 1);
    assert(heap.removeMinimum(minimum) && minimum.priority == 2);

    HashTable<int> table(7);
    assert(table.insert("NX-ONE", 11));
    assert(table.insert("NX-TWO", 22));
    assert(!table.insert("NX-ONE", 99));
    const int* found = table.find("NX-TWO");
    assert(found != nullptr && *found == 22);
    assert(table.find("MISSING") == nullptr);

    AVLTree<TreeValue> tree;
    tree.insert({30});
    tree.insert({20});
    tree.insert({10});
    tree.insert({25});
    tree.insert({40});
    assert(tree.size() == 5);
    int previous = -1;
    tree.inOrder([&previous](const TreeValue& item) {
        assert(item.key > previous);
        previous = item.key;
    });
    assert(previous == 40);
}

void testDeliveryWorkflow() {
    RouteEngine routes;
    assert(routes.load("data/locations.csv", "data/roads.csv"));

    DeliverySystem system;
    assert(system.initialize(&routes, "data/riders.csv"));
    assert(system.riders().empty());
    assert(system.dashboardSummary().availableRiders == 0);

    std::string error;
    const char* names[4] = {"First Rider", "Second Rider", "Third Rider", "Fourth Rider"};
    const char* phones[4] = {"03005550101", "03005550102", "03005550103", "03005550104"};
    const char* registrations[4] = {"TEST-101", "TEST-102", "TEST-103", "TEST-104"};
    const int locations[4] = {5, 0, 3, 14};
    for (int index = 0; index < 4; ++index) {
        RegisterRiderRequest riderRequest;
        riderRequest.name = names[index];
        riderRequest.phone = phones[index];
        riderRequest.locationId = locations[index];
        riderRequest.vehicle = index == 3 ? "Van" : "Bike";
        riderRequest.registration = registrations[index];
        riderRequest.baseCharge = 100.0 + index * 10.0;
        riderRequest.perKmCharge = 24.0 + index;
        Rider* rider = system.registerRider(riderRequest, error);
        assert(rider != nullptr);
        assert(rider->rating == 0.0);
        assert(rider->completedDeliveries == 0);
        assert(rider->earnings == 0.0);
    }
    assert(system.dashboardSummary().availableRiders == 4);

    CreateBookingRequest quickRequest;
    quickRequest.customerName = "Saad";
    quickRequest.receiverName = "Hanzala";
    quickRequest.receiverPhone = "03001234567";
    quickRequest.pickupId = 5;
    quickRequest.destinationId = 7;
    quickRequest.weightKg = 2.5;
    quickRequest.priority = ParcelPriority::Express;
    quickRequest.mode = BookingMode::QuickAssign;

    BookingCreationResult quick = system.createBooking(quickRequest);
    assert(quick.success && quick.booking != nullptr);
    assert(quick.booking->status == DeliveryStatus::RiderAssigned);
    assert(quick.booking->assignedRiderId == "RD-101");
    assert(quick.booking->history.size() == 3);
    assert(quick.booking->estimatedFare > 0.0);
    assert(quick.booking->statusChangedAtEpoch > 0);
    assert(quick.booking->nextStatusAtEpoch > quick.booking->statusChangedAtEpoch);
    assert(system.findBooking(quick.booking->trackingId) == quick.booking);

    quick.booking->nextStatusAtEpoch = 1;
    assert(!system.rateDelivery(quick.booking->trackingId, 5, error));
    assert(system.advanceDueBookings() == 1);
    assert(quick.booking->status == DeliveryStatus::RiderArriving);
    assert(system.advanceBooking(quick.booking->trackingId, error));
    assert(quick.booking->status == DeliveryStatus::PickedUp);
    assert(system.advanceBooking(quick.booking->trackingId, error));
    assert(quick.booking->status == DeliveryStatus::InTransit);
    assert(system.advanceBooking(quick.booking->trackingId, error));
    assert(quick.booking->status == DeliveryStatus::Delivered);
    assert(system.completedRecords().size() == 1);
    assert(system.findRider("RD-101")->available);
    assert(system.findRider("RD-101")->currentLocationId == 7);
    assert(system.rateDelivery(quick.booking->trackingId, 4, error));
    assert(quick.booking->customerRating == 4);
    assert(system.findRider("RD-101")->rating == 4.0);
    assert(system.findRider("RD-101")->ratingCount == 1);
    assert(!system.rateDelivery(quick.booking->trackingId, 5, error));

    CreateBookingRequest offerRequest;
    offerRequest.customerName = "Saad";
    offerRequest.receiverName = "Dawood";
    offerRequest.receiverPhone = "03111234567";
    offerRequest.pickupId = 0;
    offerRequest.destinationId = 14;
    offerRequest.weightKg = 1.0;
    offerRequest.priority = ParcelPriority::Standard;
    offerRequest.mode = BookingMode::RiderOffers;

    BookingCreationResult offers = system.createBooking(offerRequest);
    assert(offers.success && offers.booking != nullptr);
    assert(offers.booking->status == DeliveryStatus::OfferReceived);
    assert(offers.booking->offers.size() == 4);

    const std::string selectedRider = offers.booking->offers[0].riderId;
    assert(system.acceptOffer(offers.booking->trackingId, selectedRider, error));
    assert(offers.booking->status == DeliveryStatus::RiderAssigned);
    assert(offers.booking->assignedRiderId == selectedRider);
    assert(!system.findRider(selectedRider)->available);
    assert(system.cancelBooking(offers.booking->trackingId, error));
    assert(offers.booking->status == DeliveryStatus::Cancelled);
    assert(system.findRider(selectedRider)->available);

    const DashboardSummary summary = system.dashboardSummary();
    assert(summary.totalBookings == 2);
    assert(summary.deliveredBookings == 1);
    assert(summary.cancelledBookings == 1);
    assert(summary.totalRevenue > 0.0);
    assert(summary.riderPayouts > summary.platformProfit);
    assert(summary.platformProfit > 0.0);

    RegisterRiderRequest riderRequest;
    riderRequest.name = "Areeba Khan";
    riderRequest.phone = "03005550109";
    riderRequest.locationId = 4;
    riderRequest.vehicle = "Bike";
    riderRequest.registration = "NXR-109";
    riderRequest.baseCharge = 140.0;
    riderRequest.perKmCharge = 31.0;
    Rider* registered = system.registerRider(riderRequest, error);
    assert(registered != nullptr);
    assert(registered->id == "RD-105");
    assert(registered->available);
    assert(registered->rating == 0.0);
    assert(registered->currentLocationId == 4);
    assert(system.registerRider(riderRequest, error) == nullptr);
    assert(!system.updateRider("RD-105", false, 999, 155.0, 34.0, error));
    assert(system.updateRider("RD-105", false, 6, 155.0, 34.0, error));
    assert(!system.findRider("RD-105")->available);
    assert(system.findRider("RD-105")->currentLocationId == 6);
    assert(system.findRider("RD-105")->baseCharge == 155.0);
    assert(system.findRider("RD-105")->perKmCharge == 34.0);
}

void testPersistence() {
    const std::filesystem::path runtimeDirectory = "build/persistence_test";
    std::error_code cleanupError;
    std::filesystem::remove_all(runtimeDirectory, cleanupError);

    std::string trackingId;
    {
        RouteEngine routes;
        assert(routes.load("data/locations.csv", "data/roads.csv"));
        DeliverySystem system;
        assert(system.initialize(&routes, "data/riders.csv", runtimeDirectory));

        std::string error;
        RegisterRiderRequest riderRequest;
        riderRequest.name = "Persistent Rider";
        riderRequest.phone = "03005550110";
        riderRequest.locationId = 4;
        riderRequest.vehicle = "Van";
        riderRequest.registration = "NXR-110";
        riderRequest.baseCharge = 180.0;
        riderRequest.perKmCharge = 38.0;
        Rider* registered = system.registerRider(riderRequest, error);
        assert(registered != nullptr && registered->id == "RD-101");

        CreateBookingRequest request;
        request.customerName = "Persistence Test";
        request.receiverName = "Nexa Receiver";
        request.receiverPhone = "03000000000";
        request.pickupId = 5;
        request.destinationId = 7;
        request.weightKg = 1.5;
        request.priority = ParcelPriority::SameDay;
        request.mode = BookingMode::QuickAssign;

        BookingCreationResult created = system.createBooking(request);
        assert(created.success && created.booking != nullptr);
        trackingId = created.booking->trackingId;
        assert(system.advanceBooking(trackingId, error));
        assert(system.advanceBooking(trackingId, error));
        assert(system.advanceBooking(trackingId, error));
        assert(system.advanceBooking(trackingId, error));
        assert(created.booking->status == DeliveryStatus::Delivered);
        assert(system.rateDelivery(trackingId, 5, error));
        assert(std::filesystem::exists(runtimeDirectory / "bookings.db"));
        assert(std::filesystem::exists(runtimeDirectory / "history.db"));
        assert(std::filesystem::exists(runtimeDirectory / "riders.db"));
    }

    {
        RouteEngine routes;
        assert(routes.load("data/locations.csv", "data/roads.csv"));
        DeliverySystem restored;
        assert(restored.initialize(&routes, "data/riders.csv", runtimeDirectory));
        const Booking* booking = restored.findBooking(trackingId);
        assert(booking != nullptr);
        assert(booking->customerName == "Persistence Test");
        assert(booking->status == DeliveryStatus::Delivered);
        assert(booking->history.size() == 7);
        assert(restored.dashboardSummary().totalBookings == 1);
        const Rider* rider = restored.findRider(booking->assignedRiderId);
        assert(rider != nullptr && rider->available);
        assert(rider->activeTrackingId.empty());
        assert(rider->currentLocationId == 7);
        assert(rider->earnings > 0.0);
        assert(rider->rating == 5.0);
        assert(rider->ratingCount == 1);
        assert(booking->customerRating == 5);
        assert(restored.completedRecords().size() == 1);
        const Rider* registered = restored.findRider("RD-101");
        assert(registered != nullptr);
        assert(registered->name == "Persistent Rider");
        assert(registered->baseCharge == 180.0);
        assert(registered->perKmCharge == 38.0);
    }

    std::filesystem::remove_all(runtimeDirectory, cleanupError);
}
}

int main() {
    testAdditionalStructures();
    testDeliveryWorkflow();
    testPersistence();

    std::cout << "Singly linked list tests: passed\n";
    std::cout << "Binary search tests: passed\n";
    std::cout << "Doubly linked list tests: passed\n";
    std::cout << "Circular queue tests: passed\n";
    std::cout << "Min-heap tests: passed\n";
    std::cout << "Hash table tests: passed\n";
    std::cout << "AVL tree tests: passed\n";
    std::cout << "Quick Assign workflow: passed\n";
    std::cout << "Rider Offers workflow: passed\n";
    std::cout << "Tracking and delivery lifecycle: passed\n";
    std::cout << "Automatic status scheduling: passed\n";
    std::cout << "Rider registration and pricing: passed\n";
    std::cout << "Persistent storage restore: passed\n";
    return 0;
}
