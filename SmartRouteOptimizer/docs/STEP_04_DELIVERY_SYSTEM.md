# Step 4: Delivery System and DSA Modules

## Completed outcome

The project supports a complete parcel lifecycle in C++. Customers can create bookings, use automatic rider assignment or compare rider offers, track the parcel while it progresses automatically, and cancel before pickup. Admin statistics update after every operation.

## Custom structures added

### Binary search

Riders receive sequential IDs during registration, preserving sorted rider-ID order. Both mutable and read-only rider lookup call the custom generic `binarySearchIndex`, giving `O(log n)` lookup without `std::lower_bound`.

### Singly linked list

All booking records are stored in `SinglyLinkedList<Booking>`. Insertion at the tail is constant time because the list keeps both head and tail pointers.

### Doubly linked list

Each booking owns a `DoublyLinkedList<StatusEvent>`. Events can be traversed from requested to delivered or in reverse order for recent-first history.

### Circular queue

Available riders are placed in `CircularQueue<int>` when an inDrive-style request is created. The fixed circular buffer demonstrates wrap-around, full, empty, enqueue, and dequeue behavior.

### Min-heap

Quick Assign inserts eligible riders into a custom `MinHeap<RiderCandidate>`. The smallest pickup distance is removed first, with rider rating used as the tie-breaker.

### Hash table

Tracking IDs map to booking pointers in `HashTable<Booking*>`. The implementation uses the djb2 string hash and separate chaining for collisions.

### AVL tree

Delivered records are inserted into `AVLTree<CompletedRecord>` using the tracking ID as the key. Rotations keep completed records balanced and ordered.

## Booking workflows

### Quick Assign

1. Validate customer and parcel fields.
2. Calculate the Minimum Distance delivery route.
3. Insert all reachable available riders into the min-heap.
4. Remove the nearest candidate.
5. Mark that rider busy and connect the tracking ID.
6. Add the Rider Assigned event to the doubly linked history.

### Rider Offers

1. Validate the booking and calculate its route and estimated fare.
2. Rotate through available riders using the circular queue.
3. Generate up to four fictional offers using fare and pickup distance.
4. Let the customer compare rating, pickup time, and amount.
5. Mark the selected rider busy and save the accepted fare.

## Fare model

The academic fare contains:

- Rs. 120 base fare
- Rs. 35 per route kilometre
- Additional weight charge above 1 kg
- Same Day or Express priority charge
- Medium or High traffic surcharge
- Final value rounded to the nearest Rs. 5

## Delivery lifecycle

```text
Requested
  -> Searching for Rider
  -> Offer Received, for Rider Offers only
  -> Rider Assigned
  -> Rider Arriving
  -> Picked Up
  -> In Transit
  -> Delivered
```

A booking may be cancelled until pickup. Timed C++ transitions move assigned bookings through rider arrival, pickup, transit, and delivery without user clicks. Delivery releases the rider, updates the rider's location, completed count, and earnings, and inserts the completed record into the AVL tree.

## APIs added

| Method and endpoint | Purpose |
|---|---|
| `POST /api/bookings` | Create and assign a parcel request |
| `GET /api/bookings` | Return all booking records |
| `GET /api/track?id=...` | Hash-table tracking lookup |
| `POST /api/bookings/accept-offer` | Select a rider offer |
| `POST /api/bookings/advance` | Internal deterministic lifecycle/testing endpoint |
| `POST /api/bookings/rate` | Submit one genuine 1–5 rating after delivery |
| `POST /api/riders/register` | Register a rider with a required starting location and no fabricated rating |
| `POST /api/riders/update` | Update rider availability, current location, and charges |
| `POST /api/roads/add` | Add and persist a new fictional road |
| `POST /api/bookings/cancel` | Cancel before pickup |
| `GET /api/dashboard` | Return live operational totals |

## Verification completed

- Singly linked list insertion and traversal
- Doubly linked list forward and backward traversal
- Circular queue wrap-around behavior
- Min-heap priority removal
- Hash insertion, collision-safe lookup, and duplicate rejection
- AVL insertion and sorted traversal
- Nearest-rider Quick Assign
- Four generated Rider Offers and offer acceptance
- Full tracking lifecycle and cancellation rules
- Rider release, new location, earnings, completed AVL record, and feedback-based rating average
