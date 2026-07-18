# Step 6: Persistence, Testing, and Setup

## Persistent local state

Normal application runs create `data/runtime` automatically. The C++ service maintains simple tab-delimited files:

- `bookings.db` for parcel records
- `history.db` for status events
- `riders.db` for rider location, availability, deliveries, earnings, and active job
- `accounts.db` for salted password hashes and account lock state
- `auth_audit.db` for login, logout, registration, and password-change events
- `added_locations.db` for admin-created named destinations
- `added_roads.db` for admin-created named routes
- `network_audit.db` for destination and route changes

On restart, C++ loads custom locations before bookings, overlays saved rider state, recreates bookings, rebuilds indexes and AVL records, restores credentials, recalculates routes, and restores status histories.

The interface never reads these files directly.

## Automated test suites

### Route engine tests

`scripts/test_routes.sh` compiles and verifies:

- Dynamic array
- Queue
- Stack
- Graph loading
- BFS Minimum Stops
- Recursive DFS Minimum Distance
- Blocked-road rerouting
- Admin-added road validation

### Delivery-system tests

`scripts/test_delivery.sh` compiles and verifies:

- Singly and doubly linked lists
- Binary search over a custom dynamic array
- Circular queue
- Min-heap
- Hash table
- AVL tree
- Quick Assign
- Rider Offers
- Tracking and full lifecycle
- Automatic status scheduling
- Rider registration, availability, and pricing
- Save, restart, and persistent restore

### Authentication, network, and analytics tests

`scripts/test_auth_network.sh` performs more than 300 C++ checks covering:

- SHA-256 known vectors, salted password derivation, and session-token behavior
- Input, phone, name, charge, coordinate, route-name, and password validation
- Rider and admin role separation, account locks, logout, and password change
- Credential save/restart/restore
- Named destination and named route validation
- Custom graph expansion and route calculation
- Location/route persistence and sequence restoration
- Network connectivity, traffic distribution, fleet metrics, finance, and rider leaderboard

### Clean rider and feedback tests

`scripts/test_rider_feedback.sh` verifies:

- Zero riders, bookings, ratings, earnings, and completed records on a clean start
- Required starting-location validation
- Protected current-location changes and active-delivery location locks
- **Not rated** state before genuine customer feedback
- One-time post-delivery feedback and weighted rating averages
- Dynamic earnings, completion counts, and dashboard totals

### End-to-end smoke test

`scripts/smoke_test.sh` launches the real executable in clean test mode and verifies:

- Health, city, initially empty sanitized rider, route, dashboard, analytics, and frontend endpoints
- Rider/admin login plus unauthorized-access rejection
- Private rider profile and protected financial data
- The 18 km BFS and 10 km DFS comparison
- Road blocking, rerouting, and stack undo
- Quick Assign rider selection
- Full delivery progression
- Password-based rider registration, required starting location, zero fabricated rating, and protected current-location/profile updates
- Post-delivery rating creation, duplicate-rating rejection, and dynamic fleet averages
- Admin named destination and named route creation
- Automatic-update scheduling timestamps
- Product interface copy contains no course-implementation labels
- HTML, CSS, and JavaScript delivery

## Latest verified result

All route, delivery, authentication, persistence, analytics, and end-to-end tests pass with strict `-Wall -Wextra -Wpedantic` compilation.

The JavaScript syntax check passes, every JavaScript element reference maps to an existing HTML element, and a headless DOM simulation verifies the empty fleet, rider registration, starting/current-location workflow, customer booking, completed delivery, and genuine rating interface.

## Visual Studio build details

- Solution: `SmartRouteOptimizer.sln`
- Project: `backend/SmartRouteOptimizer.vcxproj`
- Platform: x64
- Language: C++17
- Windows libraries: `Ws2_32.lib` and `Shell32.lib`
- Output: `build/x64/Debug` or `build/x64/Release`

The executable locates the project root by searching parent folders, so launching from Visual Studio's backend project directory works correctly.

## Command-line options

| Option | Behavior |
|---|---|
| `--port 8090` | Use another local port |
| `--no-browser` | Do not open the default browser |
| `--no-persistence` | Run without loading or saving runtime state |
| `--no-automation` | Disable timed delivery transitions for deterministic tests |

## Final quality checklist

- Custom C++ DSA structures, not STL DSA containers
- No Dijkstra, A*, Qt, or SFML
- No third-party runtime dependencies
- No internet requirement
- Fictional city, locations, and roads; rider and operational records start empty and are user-created
- Complete Visual Studio project
- Responsive graphical interface
- Persistent booking workflow
- Dedicated clean-rider/location/feedback regression suite
- Unit, workflow, restart, and API integration coverage
- Step-by-step documentation and viva guide
