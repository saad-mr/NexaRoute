# Smart Route Optimizer

**Interface brand:** NexaRoute  
**City:** Nexa City  
**Course:** Data Structures and Algorithms  
**Core language:** C++17  
**Graphics:** HTML, CSS, vanilla JavaScript, and SVG  
**Recommended IDE:** VS Code with MSYS2 GCC/GDB on Windows  
**Also supported:** Visual Studio 2022 on Windows x64

Smart Route Optimizer is a complete offline parcel-booking and courier-delivery simulation. It combines Careem-style automatic rider assignment with inDrive-style rider offers, live tracking, automatic status progression, road controls, reports, and a fictional 15-location city map.

The browser is only the graphical interface. Routes, bookings, fares, rider matching, authentication, validation, status history, analytics, custom destinations, road changes, and persistence are handled by C++.

## Project logins

These credentials are for the local fictional classroom project:

| Workspace | Username / ID | Initial password |
|---|---|---|
| Admin | `admin` | `Nexa@2026` |

There are no pre-registered riders. Every rider enters their own profile, charges, password, vehicle, and required starting location. Credentials are salted and hashed by the C++ service; plaintext passwords are never saved.

## Run it in Visual Studio

1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Open `SmartRouteOptimizer.sln`.
3. Select **Debug** and **x64**.
4. Press **F5** or **Local Windows Debugger**.
5. Keep the C++ console open. The app opens at `http://127.0.0.1:8080`.

No Qt, SFML, Node.js, database server, web server, package installation, or internet connection is required.

## Run it in VS Code with MSYS2

The project now includes ready-made VS Code build and debug configurations.

1. Install the Microsoft **C/C++** extension.
2. Open the complete project folder in VS Code.
3. Press `Ctrl+Shift+B` to compile all C++ files.
4. Press `F5` to run with GDB.

See `docs/VSCODE_WINDOWS_SETUP.md` for the manual command and troubleshooting.

Read `START_HERE.md` for the recommended classroom demo.

## Main features

- Parcel booking with customer, receiver, weight, and priority validation
- Minimum Stops route using BFS and a custom queue
- Minimum Distance route using recursive DFS with branch-and-bound
- Animated comparison of both routes on an offline SVG city map
- Quick Assign using a custom min-heap of nearby riders
- Rider Offers using a custom circular queue
- Fare calculation using distance, weight, priority, and traffic
- Hash-table tracking lookup and a complete delivery status timeline
- Automatic rider approach, pickup, in-transit, and delivered status progression
- Animated rider/parcel movement with distinct approach, pickup, transit, and delivered colors
- Working notification center with unread counts and clickable tracking updates
- Booking cancellation before pickup
- Separate customer, rider, and admin workspaces
- Rider Login/Register choice, required starting location, editable current location, password-protected private profile, availability, pricing, earnings, and active-job view
- Post-delivery customer feedback; new riders remain **Not rated** until a real rating is submitted
- Password-protected admin workspace with named destination creation and named routes between saved locations
- Admin road blocking, traffic changes, dynamic rerouting, and undo
- C++ analytics for network reachability, traffic distribution, rider leaderboard, revenue, payouts, and platform profit
- Clean initial operational state with no fabricated riders, ratings, earnings, bookings, or completed deliveries
- Persistent bookings, histories, registered rider locations, availability, and earnings
- Responsive interface with clean new-booking forms, animated roads/routes, and non-blocked notifications

## DSA mapping

| Course concept | Project use |
|---|---|
| Dynamic array | Locations, results, riders, and offers |
| Dynamic array and binary search | Storage plus rider lookup in the sorted ID array |
| Queue | BFS frontier for Minimum Stops |
| Circular queue | Rotation of riders receiving price requests |
| Stack | Route reconstruction and road-change undo |
| Singly linked list | Master booking collection |
| Doubly linked list | Forward and backward status history |
| Recursion | DFS route exploration and AVL operations |
| Binary search tree / AVL | Balanced completed-delivery records |
| Hash table | Near constant-time tracking ID lookup |
| Hash table + linked list | Credential and session indexes in the authentication service |
| Min-heap | Nearest-rider selection |
| Min-heap | Admin rider-performance leaderboard |
| Graph adjacency list | Nexa City locations and two-way roads |
| BFS | Fewest road connections |
| BFS | Admin network-reachability analytics |
| DFS | Minimum weighted distance on the controlled map |

The application does not use Dijkstra's Algorithm or A*. This is intentional and documented honestly. Recursive DFS can be exponential in the worst case, but it is suitable for the controlled 15-location academic graph. These implementation details remain in the source and course documentation; the customer-facing interface uses normal delivery-product language.

## Architecture

```text
Browser UI
HTML + CSS + JavaScript + SVG
            |
            | Local HTTP/JSON on 127.0.0.1
            v
C++ LocalServer
            |
            +-- RouteEngine --> Graph, BFS, recursive DFS
            +-- DeliverySystem --> bookings, riders, offers, tracking
            +-- AuthService --> hashed credentials, roles, sessions
            +-- NetworkManager --> named locations and routes
            +-- AnalyticsService --> network, fleet, finance reports
            +-- Persistence --> data/runtime/*.db
            +-- CSV reference data --> locations, roads, empty rider schema
```

The backend intentionally avoids STL DSA containers such as `vector`, `queue`, `priority_queue`, `list`, `map`, and `unordered_map`. Basic infrastructure utilities such as `std::string`, streams, and `std::filesystem` are used.

## Repository structure

```text
SmartRouteOptimizer/
|-- SmartRouteOptimizer.sln
|-- backend/
|   |-- include/          Custom DSA and service headers
|   `-- src/              C++ implementations and local server
|-- frontend/
|   |-- index.html        Dashboard and dialogs
|   |-- style.css         Responsive graphics and animations
|   `-- app.js            UI behavior and API integration
|-- data/
|   |-- locations.csv     15 fictional city vertices
|   |-- roads.csv         32 weighted two-way roads
|   `-- riders.csv        Empty rider import schema (no default profiles)
|-- docs/                 Step guides and viva preparation
|-- scripts/              Linux/macOS automated test scripts
|-- tests/                Route, structure, workflow, and persistence tests
|-- CMakeLists.txt
|-- START_HERE.md
`-- README.md
```

## Completed milestones

- [x] Step 1: Project blueprint, city model, DSA mapping, and scope
- [x] Step 2: Visual Studio solution and C++/browser foundation
- [x] Step 3: Custom structures, graph, BFS, and recursive DFS
- [x] Step 4: Booking, rider, offers, tracking, and admin logic
- [x] Step 5: High-quality interface and animated offline map
- [x] Step 6: Persistence, integration tests, documentation, and packaging

## Verification

From a Bash terminal in the project root:

```bash
./scripts/test_routes.sh
./scripts/test_delivery.sh
./scripts/test_auth_network.sh
./scripts/test_rider_feedback.sh
./scripts/smoke_test.sh
```

The suites verify the clean empty state, all custom structures, both route calculations, authentication and role separation, password hashing, named destinations/routes, analytics, rider registration, starting/current-location changes, pricing, automatic lifecycle scheduling, dynamic rerouting, booking modes, persistent restore, APIs, and frontend delivery.

See `docs/LANGUAGE_REPORT.md` for the reproducible C++/JavaScript executable-source measurement.

## Documentation

- `docs/STEP_01_BLUEPRINT.md`
- `docs/STEP_02_VISUAL_STUDIO_FOUNDATION.md`
- `docs/STEP_03_ROUTE_ENGINE.md`
- `docs/STEP_04_DELIVERY_SYSTEM.md`
- `docs/STEP_05_INTERFACE_AND_ADMIN.md`
- `docs/STEP_06_TESTING_AND_SETUP.md`
- `docs/VIVA_GUIDE.md`
- `docs/VSCODE_WINDOWS_SETUP.md`
