# Viva Guide

## 30-second introduction

“Smart Route Optimizer is an offline C++ DSA project for parcel booking in the fictional Nexa City. The browser provides the graphics, but C++ owns the graph, route algorithms, rider assignment, authentication, bookings, tracking, named locations, analytics, and persistence. BFS finds minimum road connections, while recursive DFS with branch-and-bound finds minimum total distance.”

## Best algorithm example

Use Airport Terminal to Industrial Zone.

```text
BFS Minimum Stops:
Airport Terminal -> Industrial Zone
1 road, 18 km

Recursive DFS Minimum Distance:
Airport Terminal -> City Mall -> Industrial Zone
2 roads, 10 km
```

The key statement is: BFS minimizes edges in an unweighted interpretation. It does not minimize weighted kilometres.

## Common questions

### Why did you not use Dijkstra or A*?

They are outside this course outline. The project uses concepts that are taught: graph adjacency lists, BFS, DFS, recursion, queue, stack, heap, hash table, linked lists, and AVL tree. DFS minimum distance is practical here because the fictional graph has only 15 vertices and branch-and-bound cuts paths already longer than the best result.

### What is the DFS worst-case complexity?

It can be exponential because it may explore many simple paths. The project does not claim otherwise. The controlled graph size makes the trade-off acceptable for demonstration.

### Where is C++ used if the screen is HTML?

HTML and CSS draw the interface. JavaScript sends local requests. The native C++ executable serves the files and calculates every route, fare, rider match, status change, credential/session lookup, road update, report, and saved record.

### How are Rider and Admin panels protected?

The C++ `AuthService` stores salted iterated SHA-256 password hashes, never plaintext passwords. Credentials and sessions use custom linked lists plus custom hash-table indexes. Every protected endpoint checks the session token and role; a rider can only update the matching rider ID.

### How can the admin create a new destination?

`NetworkManager` validates the destination name, type, anchor, route name, distance, and time. The graph expands its raw adjacency array, assigns the next location ID, generates a unique internal route ID, and inserts a named two-way edge. Both records are persisted and restored in the correct order.

### What does the analytics service demonstrate?

It uses BFS with the custom circular queue to measure reachable locations and a custom min-heap to build the rider leaderboard. It also calculates unique-road traffic distribution, fleet availability, status counts, revenue, payouts, and profit in C++.

### Why does a new rider show “Not rated”?

C++ initializes the rating count to zero and the API returns no numeric rating until a customer rates a completed delivery. `rateDelivery` rejects early or duplicate feedback and calculates a weighted average from genuine submitted ratings.

### Why use an adjacency list?

The city graph is sparse: 15 vertices and 32 roads. An adjacency list stores only existing roads and supports BFS and DFS in `O(V + E)` traversal time.

### Why use a min-heap?

Quick Assign needs the closest reachable rider. Candidates are inserted using pickup distance as priority, so removing the minimum returns the nearest rider efficiently.

### Where is binary search used?

Dynamically registered riders receive sequential IDs, which keeps the rider array in sorted ID order. `findRider` uses the custom generic binary search for both editable and read-only lookup in `O(log n)` time.

### How does the hash table handle collisions?

It uses separate chaining. Each bucket points to a linked chain of entries with the same bucket index. The djb2 hash converts tracking text into a bucket number.

### Why use an AVL tree?

Completed tracking records remain balanced and ordered. AVL rotations prevent the binary search tree from degrading into a long linked list after ordered insertions.

### What does the custom stack do?

It reverses predecessor paths where needed and stores previous road states. Admin undo pops the latest road change, demonstrating LIFO behavior.

### How does data survive a restart?

C++ saves bookings, histories, and rider state into local tab-delimited files. Startup reconstructs linked-list bookings, hash references, histories, and AVL completed records.

## Complexity summary

| Operation | Complexity |
|---|---|
| BFS route | `O(V + E)` |
| Binary-search rider lookup | `O(log n)` |
| Recursive DFS route | Exponential worst case |
| Hash lookup | `O(1)` average, `O(n)` worst case |
| AVL insert/search | `O(log n)` |
| Heap insert/remove-min | `O(log n)` |
| Linked-list tail insert | `O(1)` |
| Queue enqueue/dequeue | `O(1)` amortized |
| Stack push/pop | `O(1)` amortized |

## Demonstration order

1. Show the Nexa City map and connected graph.
2. Compare Airport Terminal to Industrial Zone.
3. Switch between both route cards.
4. Book once with Quick Assign.
5. Track the parcel and explain that C++ schedules each automatic status transition while JavaScript animates the returned state.
6. Book once with Rider Offers.
7. Block R22 and explain dynamic graph state.
8. Undo the block using the stack.
9. Open Rider, register a new account with a starting location, change its current location, sign out, and log in with the generated rider ID.
10. Log in as Admin, create a named destination and route, then show protected analytics and financial reports.
11. Explain that course concepts are documented in the source and viva guide but are intentionally not advertised in the customer-facing product UI.
12. Restart and show persistent accounts, locations, routes, riders, and bookings.

## Statements to avoid

- Do not say BFS always finds the shortest kilometre route.
- Do not say DFS is faster than Dijkstra.
- Do not say the project uses a real GPS or live traffic service.
- Do not say HTML implements the DSA algorithms.
- Do not hide the DFS worst-case complexity.

The city, base locations, roads, and traffic are fictional academic reference data. Rider accounts, registrations, charges, ratings, earnings, and deliveries are not preloaded; they appear only after project use.
