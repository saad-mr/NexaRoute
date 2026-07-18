# Smart Route Optimizer

Smart Route Optimizer is a fictional parcel booking and delivery management system created as a Data Structures and Algorithms project. It provides customer, rider, and admin panels with route planning, automatic delivery progress, live map animation, and delivery reports.

The main application logic is implemented in **C++17**, while **HTML, CSS, JavaScript, and SVG** provide the graphical interface.

## Main Features

- Book and track parcels
- Compare minimum-stop and minimum-distance routes
- Automatically assign the nearest available rider
- Allow riders to register, log in, set charges, and change location
- Animate rider movement from pickup to delivery
- Show notifications and delivery status updates
- Allow customers to rate riders after delivery
- Protect rider and admin panels with login
- Let admins create locations and named routes
- Let admins block roads, change traffic, and view reports
- Save registered riders, bookings, locations, and delivery history locally
- Start with no fake riders, ratings, earnings, or bookings

## User Panels

### Customer Panel

Customers can plan routes, book parcels, track deliveries, cancel eligible bookings, and rate riders after successful delivery.

### Rider Panel

Riders can register or log in, select a starting location, update their current location, set delivery charges, manage availability, and view active jobs and earnings.

### Admin Panel

The admin can manage roads, traffic, locations, and named routes. The panel also provides fleet, delivery, revenue, payout, and profit reports.

## DSA Concepts Used

- Dynamic arrays
- Singly and doubly linked lists
- Stack
- Queue and circular queue
- Binary search
- Hash table
- Min-heap
- Binary search tree and AVL tree
- Graph using an adjacency list
- Breadth-First Search (BFS)
- Recursive Depth-First Search (DFS)

The project does **not** use Dijkstra's Algorithm or A*. BFS finds the route with the fewest connections, while recursive DFS finds the minimum-distance route on the small fictional map.

## Technologies

| Part | Technology |
|---|---|
| Core logic and DSA | C++17 |
| Interface | HTML, CSS and JavaScript |
| Map and animations | SVG and CSS |
| Communication | Local HTTP/JSON server |
| Data storage | Local CSV and runtime files |

No Qt, Node.js, database server, or internet connection is required.

## How to Run in VS Code

### Requirements

- VS Code
- Microsoft C/C++ extension
- C++17-compatible GCC compiler
- GDB debugger

### Steps

1. Open the complete project folder in VS Code.
2. Press `Ctrl + Shift + B` and select **Build Smart Route Optimizer**.
3. Press `F5` and select **Run Smart Route Optimizer**.
4. Keep the terminal window open.
5. The application will open at `http://127.0.0.1:8080`.

If the browser does not open automatically, enter the address manually.

## How to Run in Visual Studio 2022

1. Install the **Desktop development with C++** workload.
2. Open `SmartRouteOptimizer.sln`.
3. Select **Debug** and **x64**.
4. Press `F5` or click **Local Windows Debugger**.

## Admin Login

| Username | Password |
|---|---|
| `admin` | `Nexa@2026` |

There are no default rider accounts. Riders must register before logging in.

## Project Structure

```text
SmartRouteOptimizer/
|-- backend/       C++ source code and DSA implementations
|-- frontend/      HTML, CSS, JavaScript and SVG interface
|-- data/          Fictional locations, roads and empty rider data
|-- docs/          Setup instructions and project documentation
|-- scripts/       Build and testing scripts
|-- tests/         C++ tests
|-- CMakeLists.txt
|-- SmartRouteOptimizer.sln
|-- START_HERE.md
`-- README.md
```

## Important Note

This is an offline academic project based on a fictional city and fictional delivery service. It is designed to demonstrate practical use of C++ and core DSA concepts through a modern graphical application.
