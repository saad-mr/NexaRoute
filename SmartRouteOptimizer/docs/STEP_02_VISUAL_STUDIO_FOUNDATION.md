# Step 2: Visual Studio and Interface Foundation

## Completed outcome

The project now contains a runnable Visual Studio 2022 solution. The native C++ executable starts an offline local server, serves the browser interface, loads the city CSV files and empty rider schema, and exposes verified API endpoints.

## How to run on Windows

1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Open `SmartRouteOptimizer.sln`.
3. Select `Debug` and `x64` from the top toolbar.
4. Make sure **SmartRouteOptimizer** is the startup project.
5. Press **Local Windows Debugger** or `F5`.
6. Keep the console window open while using the application.
7. The default browser will open `http://127.0.0.1:8080` automatically.
8. Press `Ctrl+C` in the console when you want to stop the application.

No Qt, SFML, web server, database, Node.js, or internet connection is required to run the application.

## Foundation components

### Visual Studio solution

- `SmartRouteOptimizer.sln`
- x64 Debug configuration
- x64 Release configuration
- C++17 language standard
- Windows socket and browser-launch libraries linked automatically
- Build output placed under the root `build` folder

### Native C++ server

The `LocalServer` class performs the following work:

- Binds only to the computer's local address
- Listens on port 8080
- Serves HTML, CSS, JavaScript, SVG-compatible content, fonts, and images
- Rejects invalid path traversal requests
- Returns suitable content types
- Loads city CSV files and an intentionally empty rider import schema
- Produces JSON without a third-party JSON package
- Closes each connection after returning a response

### Verified endpoints

| Endpoint | Purpose |
|---|---|
| `/api/health` | Confirms that the C++ engine is running |
| `/api/city` | Returns all 15 locations and 32 roads |
| `/api/riders` | Returns dynamically registered riders and availability; initially empty |

### Browser dashboard

The first interface version contains:

- Professional sidebar navigation
- Network status and C++ connection indicator
- Responsive statistics cards
- SVG city map generated from C++ data
- Traffic-colored roads and distance labels
- All 15 city location markers
- Animated markers for available riders
- Parcel-planning form shell
- Quick Assign and Rider Offers selections
- Implementation-progress panel
- Graph-health panel
- Validation and toast notifications

## Separation of responsibilities

| Layer | Current responsibility |
|---|---|
| HTML | Interface structure |
| CSS | Layout, colors, responsiveness, and animations |
| JavaScript | Fetch C++ data and draw the map |
| C++ | Run the application, serve files, load records, and produce API data |

Route calculation has deliberately not been placed in JavaScript. It will be implemented in C++ during Step 3.

## Verification completed

The foundation was compiled with strict warnings and tested through an automated smoke test. The test confirmed:

- C++ server startup
- Health API response
- 15 returned locations
- 32 returned roads
- Zero riders on a clean start, followed by dynamically registered profiles
- Successful HTML loading
- Successful CSS loading
- Successful JavaScript loading
- No STL DSA containers in the backend source

## Troubleshooting

### Port 8080 is unavailable

Close the application using that port, or open project properties and temporarily run the executable with:

```text
--port 8090
```

### Browser does not open

Keep the C++ console running and manually open:

```text
http://127.0.0.1:8080
```

### Frontend or data folders cannot be found

Start the project through the provided solution and do not move the executable away from the project folder by itself. The program searches parent folders for `frontend/index.html` and `data/locations.csv`.

## Next step

Step 3 will add the custom C++ data structures, graph loader, BFS Minimum Stops route, recursive DFS Minimum Distance route, route comparison endpoint, and automated algorithm tests.
