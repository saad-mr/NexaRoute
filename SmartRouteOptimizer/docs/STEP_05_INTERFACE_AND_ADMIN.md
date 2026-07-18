# Step 5: Graphics, Tracking, and Admin Controls

## Completed outcome

NexaRoute provides a polished offline dashboard built with HTML, CSS, vanilla JavaScript, and SVG. It is served directly by the C++ executable and contains no external fonts, packages, map services, or CDN dependencies.

## Visual system

- Responsive operations dashboard with sidebar navigation
- Four live summary cards
- Fictional SVG map generated from C++ city JSON
- Traffic colors for low, medium, high, and blocked roads
- Distance view that emphasizes road weights
- Animated rider markers, parcel movement, and route drawing
- Location labels, road distances, legends, and service-coverage display
- Separate customer, rider, and admin workspaces
- Booking, offer, confirmation, tracking, road-control, fleet, history, report, and notification dialogs
- Loading states, status chips, toasts, validation, and mobile breakpoints

## Route comparison interaction

Customers can type a saved location name or select it from suggestions. The frontend sends the names, C++ resolves them to graph IDs, and C++ returns Minimum Stops and Minimum Distance results. JavaScript displays their distance, time, stops, compact path, and animated SVG polyline.

Changing the selected route card changes only the visualization. It never recalculates an algorithm in JavaScript.

## Tracking interface

The tracking dialog uses `/api/track`. The returned record includes:

- Tracking ID and current status
- Pickup, destination, receiver, weight, and priority
- Accepted fare and assigned rider
- Delivery route
- Every status event with time and note

The interface polls live C++ state and updates automatically. The rider starts from their current location, approaches the pickup in amber, confirms pickup in green, travels toward the destination in blue, and finishes in a delivered state. The customer can cancel only while the request is still eligible.

Toasts and notification cards use a higher visual layer than modal backdrops, so a status update remains readable even while another dialog is open.

## Road-control interface

The admin workspace requires a valid C++ session. An authorized admin can:

- Block or unblock it
- Change its traffic level
- Recalculate active booking routes
- Undo the latest road change
- Add a user-named route between two saved fictional locations
- Create a user-named destination, connect it through a named route, and persist both

The backend stores the previous road state in a custom stack. Both directions of a two-way road change together.

## Management modules

### Rider workspace

Selecting Rider first presents **Login to profile** and **Register as a rider**. A clean project has no rider accounts. New riders create a password, enter their own charges, and choose a required starting location. After C++ verifies the session, a rider can view an active delivery and change availability, current location, base charge, and per-kilometre charge. Location changes are blocked while an active parcel is assigned. A rider session cannot update another rider ID.

### Rider directory

This protected admin module shows only dynamically registered riders, availability, genuine ratings when available, private vehicle details, current locations, and fleet totals. Empty-state cards appear before the first registration. New riders display **Not rated** until a customer submits feedback after a completed delivery. The public rider endpoint returns only sanitized map data.

### Delivery history

Shows persistent booking records and summary values. Selecting a record opens its live tracking history.

### Reports

Shows C++-calculated gross revenue, rider payouts, platform profit, delivery rate, fleet readiness, reachable locations, traffic distribution, rider leaderboard, and booking-status distribution. Academic implementation details are deliberately kept out of the product-facing dashboard.

## Separation of responsibilities

| Layer | Responsibility |
|---|---|
| HTML | Accessible interface structure and SVG layers |
| CSS | Layout, visual identity, responsive states, and animation |
| JavaScript | Input handling, API requests, and DOM/SVG rendering |
| C++ | Validation, authentication, authorization, algorithms, analytics, data structures, state changes, and files |

This separation allows strong graphics without weakening the C++ DSA requirement.
