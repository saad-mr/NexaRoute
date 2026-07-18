# Start Here

## 1. Open and run

1. Extract the project without changing its internal folders.
2. In VS Code, open the complete `SmartRouteOptimizer` folder.
3. Press `Ctrl+Shift+B` to build with GCC 16.1.0.
4. Press `F5` to run with GDB 17.2. Visual Studio users can instead open `SmartRouteOptimizer.sln`.
5. Allow local network access if Windows asks.
6. Keep the console open while the browser dashboard is running.

The program searches upward for the `frontend` and `data` folders, starts a local C++ server, and opens `http://127.0.0.1:8080`.

## 2. Best five-minute demonstration

1. Open **Rider**, select **Register as a rider**, and enter a new profile. Choose **Airport Terminal** as the required starting location and create your own charges and password.
2. Copy the generated rider ID. In the rider profile, change the current location to **City Mall**, save it, then sign out and log in again with the new ID and password.
3. Return to **Customer**. In **Plan a delivery**, type or select **Airport Terminal** as pickup and **Industrial Zone** as destination.
4. Press **Compare route options** and explain that BFS chooses the direct 18 km road because it has one connection, while recursive DFS finds the 10 km route through City Mall.
5. Press **Book this delivery**, enter receiver details, and use **Quick Assign**.
6. Copy the generated tracking ID, close the confirmation dialog, and watch the rider automatically approach, collect, transport, and deliver the parcel on the map. After delivery, submit a genuine customer rating and show the rider profile update from **Not rated**.
7. Optionally register more riders, then create a second booking with **Rider Offers** and choose one of the dynamically returned offers.
8. Select **Admin** and use `admin` / `Nexa@2026`.
9. Create **Green Valley Depot**, connect it from an existing location using the named route **Green Valley Road**, then return to Customer and route to it by name.
10. Open **Road control**, block road R22, and show the route recalculate. Then use **Undo last change**.
11. Open the protected fleet, delivery history, and financial reports. They contain only data produced during this run.
12. Open the notification center and select a delivery update to jump directly to tracking.
13. Close and restart the program to show that user-created accounts, bookings, rider locations, pricing, destinations, and named routes were restored.

## 3. Important explanation

- BFS optimizes the number of edges, not kilometres.
- Recursive DFS explores simple routes and stores the minimum total distance.
- The graph is intentionally small, so DFS is acceptable for this academic project.
- The browser draws results but does not calculate routes.
- C++ validates rider/admin sessions before protected data or controls are returned.
- A fresh installation contains no rider profiles, ratings, earnings, bookings, or delivery records.
- New riders begin as **Not rated**; completed counts and earnings update only through real project activity.
- Admin-created locations and route names become part of the graph and remain available after restart.
- Every assessed DSA structure is implemented manually in C++.

## 4. Useful launch options

```text
--port 8090
--no-browser
--no-persistence
--no-automation
```

`--no-persistence` is used by integration tests so test runs always start clean. `--no-automation` is only for deterministic API tests; normal runs should leave automation enabled.

## 5. Troubleshooting

- If the browser does not open, manually visit `http://127.0.0.1:8080`.
- If port 8080 is busy, add `--port 8090` in the Visual Studio debugging command arguments.
- If Windows Firewall appears, allow access only for private/local networks.
- If the app cannot locate its files, run the provided solution and keep the original folder structure.
