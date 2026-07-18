# Step 1: Complete Project Blueprint

## 1. Project identity

### Academic title

**Smart Route Optimizer: A DSA-Based Parcel Booking, Rider Assignment, and Route Management System**

### Interface brand

**NexaRoute**

### One-line description

An offline C++ application that lets customers book parcels, receive courier offers, calculate suitable routes, and watch delivery progress on an animated fictional city map.

## 2. Project purpose

The project demonstrates how data structures solve connected real-world problems. It is not only a parcel-management form. Booking queues, rider selection, tracking, road networks, route searching, record indexing, sorting, and dynamic road blocking are all connected to manually implemented DSA concepts.

The interface will resemble a modern delivery application, while the C++ source code will clearly show the academic concepts being graded.

## 3. Scope boundaries

### Included

- Offline customer, rider, and admin accounts
- Parcel booking and fare estimation
- Careem-style automatic rider assignment
- inDrive-style rider offers
- Minimum-stop and minimum-distance routes
- Rider and parcel simulation on a visual map
- Express, same-day, and standard booking priorities
- Parcel tracking using a unique tracking ID
- Road blocking, unblocking, and traffic changes
- Delivery history and admin reports
- File-based permanent storage

### Not included

- Real payments or banking
- Real GPS tracking
- Google Maps or another online map service
- Production-level authentication or encryption
- Live communication with real riders
- Dijkstra's Algorithm or A*
- Qt, SFML, or another desktop GUI framework

## 4. Technical architecture

### 4.1 Presentation layer

The browser interface will use:

- HTML for screens, forms, cards, tables, and dialogs
- CSS for layout, colors, responsive sizing, transitions, and animation
- JavaScript for interface events and requests to C++
- SVG for the city map, roads, nodes, route lines, and moving rider icons

JavaScript will not calculate routes or replace the DSA implementation. It will only collect input, call the C++ engine, and display returned results.

### 4.2 Local connection layer

A small native C++ local server will:

- Listen only on `127.0.0.1:8080`
- Serve the frontend files
- Receive requests from JavaScript
- Validate input
- Call C++ services and data structures
- Return compact JSON responses
- Open the interface in the default browser when the executable starts

The server will use Windows networking support available in Visual Studio. The finished application will not require a separate online server.

### 4.3 C++ application layer

The core services will include:

| Service | Responsibility |
|---|---|
| AuthenticationService | Local registration, login, and role verification |
| BookingService | Create, validate, cancel, prioritize, and store bookings |
| RouteService | BFS, DFS, route comparison, and blocked-road handling |
| RiderService | Availability, rider search, automatic assignment, and offers |
| TrackingService | Status changes and tracking history |
| AdminService | Road controls, searching, reports, and dashboard statistics |
| StorageService | Read and write application data files |

### 4.4 DSA layer

Every assessed structure will be kept in a separate C++ class so it can be explained during the viva.

| Course concept | Project use | Expected complexity |
|---|---|---|
| Fixed and dynamic arrays | Locations, graph rows, and internal storage | Access: O(1) |
| Binary Search | Search a sorted location or completed-booking array | O(log n) |
| Stack | Undo recent admin road changes | Push/pop: O(1) |
| Queue | Pending parcel booking requests | Enqueue/dequeue: O(1) |
| Circular Queue | Rotate new requests among available riders | Enqueue/dequeue: O(1) |
| Singly Linked List | Active bookings | Insert: O(1), search: O(n) |
| Doubly Linked List | Parcel status timeline and history navigation | Insert: O(1) |
| Recursion | DFS route exploration and tree traversal | Depends on graph/tree |
| Binary Search Tree | Completed bookings in sorted ID order | Average O(log n) |
| AVL Tree | Balanced searchable delivery archive | O(log n) |
| Hash Table | Tracking ID lookup | Average O(1) |
| Min-Heap | Express parcel scheduling and nearest-rider shortlist | Insert/remove: O(log n) |
| Heap Sort | Admin delivery and rider performance reports | O(n log n) |
| Graph | Nexa City road network | V locations, E roads |
| BFS | Route with the fewest road connections | O(V + E) |
| DFS | Explore alternatives and find minimum weighted distance | Exponential worst case |

Huffman Encoding is intentionally excluded because adding compression to this small offline dataset would be forced and would not improve the central delivery workflow.

## 5. Route engine

### 5.1 Graph representation

Nexa City is an undirected weighted graph:

- A vertex represents a named city location.
- An edge represents a two-way road.
- Each road stores distance, base travel time, traffic level, and blocked status.
- The primary representation will be a manually implemented adjacency list.
- An adjacency matrix can also be generated for the admin graph view and course demonstration.

### 5.2 Minimum Stops mode

BFS will calculate the path containing the lowest number of road connections.

Example interpretation:

`Warehouse -> Lake View -> Central Hub -> Hospital`

This route has three road connections. It may not have the lowest kilometre total, so the interface will label it **Minimum Stops**, not **Minimum Distance**.

### 5.3 Minimum Distance mode

Recursive DFS will explore every valid simple route and calculate its distance. The engine will keep the route with the smallest total.

The search will use the following safe optimizations without changing its result:

- Never revisit a location already in the current path.
- Ignore blocked roads.
- Stop exploring a branch when its distance is already greater than the best known distance.
- Visit shorter neighboring roads first to obtain a useful best distance early.

This is branch-and-bound DFS. It uses concepts from the course while remaining practical for the controlled 15-location map.

### 5.4 Traffic handling

Traffic will affect the displayed estimated time and fare. Because weighted fastest-path algorithms are outside the course outline, the system will not claim that it calculates the globally fastest time route.

Traffic levels:

| Level | Time multiplier | Visual color |
|---|---:|---|
| Low | 1.00 | Green |
| Medium | 1.25 | Amber |
| High | 1.60 | Red |

### 5.5 Road blocking

When the admin blocks a road:

1. The graph edge remains stored but is marked unavailable.
2. BFS and DFS ignore the edge.
3. Any affected active delivery is recalculated.
4. The frontend changes the road to a dashed red line.
5. The change is pushed onto an undo stack.

## 6. User roles and workflows

### 6.1 Customer workflow

1. Register or log in.
2. Open **Book a Parcel**.
3. Select pickup and destination locations.
4. Enter receiver, parcel type, weight, and priority.
5. Compare Minimum Stops and Minimum Distance routes.
6. View estimated distance, time, and fare.
7. Choose automatic assignment or rider-offer mode.
8. Confirm a rider.
9. Track the rider and parcel on the map.
10. View the completed delivery in history.

### 6.2 Rider workflow

1. Log in and switch availability on.
2. Receive a booking through the circular notification queue.
3. Review pickup, destination, parcel, and estimated distance.
4. Accept the automatic assignment or submit an offer.
5. Travel to pickup.
6. Update status to Picked Up.
7. Follow the highlighted delivery route.
8. Mark the parcel Delivered.
9. View earnings and completed jobs.

### 6.3 Admin workflow

1. Log in to the system dashboard.
2. View total, active, pending, delivered, and cancelled bookings.
3. Search any parcel using its tracking ID.
4. View the road network and current traffic.
5. Block or unblock roads.
6. Undo the latest road change.
7. View available and busy riders.
8. Sort reports by fare, distance, date, rating, or delivery count.

## 7. Booking modes

### 7.1 Quick Assign

This mode behaves like Careem:

- The system finds available riders.
- It calculates each rider's distance to the pickup location.
- Riders are inserted into a min-heap.
- The best candidate is removed from the heap and assigned.
- If that rider rejects the request, the next candidate is selected.

### 7.2 Rider Offers

This mode behaves like inDrive:

- The customer enters an optional preferred price.
- Nearby riders receive the request.
- Riders submit an offer amount and pickup time.
- Offers appear as cards containing rider name, rating, distance, time, and price.
- The customer chooses an offer.

## 8. Parcel priority

| Priority | Heap value | Meaning |
|---|---:|---|
| Express | 1 | Process first and apply express fare |
| Same Day | 2 | Process after express parcels |
| Standard | 3 | Normal processing order |

Parcels with equal priority follow first-in, first-out order using a sequence number.

## 9. Booking lifecycle

Allowed statuses are:

1. Requested
2. Searching for Rider
3. Offer Received
4. Rider Assigned
5. Rider Arriving
6. Picked Up
7. In Transit
8. Delivered

Alternative final status:

- Cancelled

Invalid jumps will be rejected. For example, a Requested parcel cannot immediately become Delivered.

## 10. Tracking IDs

Tracking IDs will follow this readable pattern:

`NX-YYYYMMDD-####`

Example:

`NX-20260718-0042`

The complete ID will be stored as the key in a custom hash table. Collision handling will use separate chaining with manually implemented linked nodes.

## 11. Fare model

The first version will use a transparent academic formula:

`Fare = Base Fare + Distance Charge + Weight Charge + Priority Charge + Traffic Adjustment`

Initial values:

| Component | Value |
|---|---:|
| Base fare | Rs. 120 |
| Per kilometre | Rs. 35 |
| Per kilogram above 1 kg | Rs. 20 |
| Same Day charge | Rs. 80 |
| Express charge | Rs. 160 |
| Medium traffic adjustment | 5% |
| High traffic adjustment | 12% |

The inDrive-style mode treats this result as the system estimate, while riders may submit their own offers.

## 12. Data records

### 12.1 User

- User ID
- Name
- Phone or username
- Password for local demonstration
- Role
- Active status

### 12.2 Customer

- User fields
- Default contact information
- Booking history

### 12.3 Rider

- User fields
- Current location
- Vehicle type and registration
- Rating
- Available or busy status
- Completed deliveries
- Current earnings

### 12.4 Parcel booking

- Tracking ID
- Customer ID
- Rider ID
- Pickup and destination IDs
- Receiver name and phone
- Parcel type
- Weight
- Priority
- Booking mode
- Estimated distance, time, and fare
- Accepted offer
- Current status
- Created timestamp
- Route location IDs

### 12.5 Road

- Road ID
- User-facing route name
- Start location ID
- End location ID
- Distance
- Base travel time
- Traffic level
- Blocked status

## 13. Interface screens

### Public screens

1. Welcome and role selection
2. Login
3. Registration

### Customer screens

4. Customer dashboard
5. Book a parcel
6. Route comparison
7. Rider search and offers
8. Live tracking
9. Delivery history
10. Booking details

### Rider screens

11. Rider dashboard
12. Incoming requests
13. Submit offer
14. Active delivery navigation
15. Earnings and delivery history

### Admin screens

16. Admin overview
17. Live operations map
18. Road and traffic controls
19. Parcel search and management
20. Rider management
21. Reports and statistics

## 14. Visual direction

The application will use a clean logistics-dashboard appearance rather than a game or cyberpunk style.

### Color palette

| Purpose | Color |
|---|---|
| Main background | `#F4F7FB` |
| Dark navigation | `#0F172A` |
| Primary blue | `#2563EB` |
| Delivery green | `#16A34A` |
| Warning amber | `#F59E0B` |
| Blocked road red | `#DC2626` |
| Card background | `#FFFFFF` |
| Main text | `#172033` |

### Interface characteristics

- Large map area with floating information cards
- Rounded but restrained cards and buttons
- Clear visual hierarchy and readable labels
- Smooth route drawing and vehicle movement
- Responsive 16:9 desktop layout
- Small status animations instead of distracting effects
- Consistent icons and spacing

## 15. Fictional city design

Nexa City contains 15 vertices. Coordinates are stored for drawing only and do not control route distance. Road distances come from `roads.csv`.

| ID | Location | Type |
|---:|---|---|
| 0 | Central Courier Hub | Hub |
| 1 | University Square | Education |
| 2 | General Hospital | Health |
| 3 | City Mall | Commercial |
| 4 | Railway Station | Transport |
| 5 | Airport Terminal | Transport |
| 6 | Technology Park | Business |
| 7 | Industrial Zone | Industrial |
| 8 | Green Market | Commercial |
| 9 | Old Town | Residential |
| 10 | Lake View | Residential |
| 11 | North Residential Area | Residential |
| 12 | South Residential Area | Residential |
| 13 | Bus Terminal | Transport |
| 14 | Parcel Warehouse | Warehouse |

## 16. Validation rules

- Pickup and destination must be different.
- Both locations must exist and be active.
- Weight must be greater than zero and no more than 30 kg.
- Customer and receiver names cannot be empty.
- A parcel cannot be assigned to an unavailable rider.
- A blocked road cannot appear in a calculated route.
- A customer can cancel only before Picked Up.
- A rider cannot handle two active deliveries unless multi-stop mode is enabled later.
- Tracking IDs must be unique.
- Rider offers must be positive numbers.

## 17. Acceptance criteria

Step 1 is approved when:

- The project uses only concepts covered by the supplied course outline.
- The distinction between BFS and weighted DFS results is accurate.
- The city graph is connected and supports alternative routes.
- Every major DSA structure has a meaningful application.
- HTML/CSS/JavaScript are restricted to presentation and interaction.
- C++ owns booking, routing, rider, tracking, and admin logic.
- The application remains fully usable without internet access.

## 18. Next implementation step

Step 2 will create:

- A Visual Studio 2022 solution and C++17 project
- The native local server skeleton
- A `/health` API response from C++
- Static frontend file serving
- A polished initial dashboard shell
- An SVG version of the Nexa City map
- One-click launch from Visual Studio

Step 2 will be considered successful when pressing **Local Windows Debugger** opens a browser and displays the NexaRoute dashboard with a confirmed C++ backend connection.
