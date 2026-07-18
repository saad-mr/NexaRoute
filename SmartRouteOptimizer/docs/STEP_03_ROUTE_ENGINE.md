# Step 3: Custom Graph and Route Engine

## Completed outcome

The application now calculates and compares real routes in C++. The frontend sends only the selected location IDs. C++ loads the graph, runs BFS and recursive DFS, calculates route metrics, and returns the ordered path to the interface.

## Implemented custom structures

### DynamicArray

`DynamicArray<T>` is a template class that provides:

- Dynamic memory allocation
- Automatic capacity growth
- Deep-copy constructor
- Copy assignment
- Move constructor and move assignment
- Constant-time indexed access
- `pushBack`, `popBack`, `reserve`, and `clear`

It replaces `std::vector` in the route engine.

### Queue

`Queue<T>` is a dynamically growing circular queue. It provides:

- First-in, first-out ordering
- Constant-time enqueue
- Constant-time dequeue
- Front peeking
- Automatic capacity growth while preserving order

BFS uses `Queue<int>` for pending graph vertices.

### Stack

`Stack<T>` is implemented over the custom dynamic array. BFS uses it to reverse the predecessor chain into a correctly ordered route from pickup to destination.

### Graph

The `Graph` class uses a manually allocated adjacency list:

- `EdgeNode**` stores the head of each adjacency list.
- Each road is represented by two linked edge nodes because roads are two-way.
- Each edge stores road ID, destination, distance, base time, traffic, and blocked status.
- The graph owns and deletes every edge node.
- No `vector`, `list`, `map`, or `unordered_map` is used.

## BFS: Minimum Stops

BFS treats every road connection as one step.

### Process

1. Mark the pickup vertex visited.
2. Insert it into the custom queue.
3. Remove the front vertex.
4. Visit each unblocked, unvisited neighbor.
5. Record the previous vertex for every discovered neighbor.
6. Stop after reaching the destination.
7. Follow the predecessor array backward.
8. Use the custom stack to reverse the path.
9. Calculate the final distance and estimated time.

### Complexity

`O(V + E)`

Here, `V` is the number of locations and `E` is the number of roads.

## Recursive DFS: Minimum Distance

DFS explores simple routes and compares their kilometre totals.

### Process

1. Add the pickup location to the current path.
2. Recursively visit every unblocked neighbor that is not already in the path.
3. Add each road's distance to the current total.
4. When the destination is reached, compare the route with the best known route.
5. Copy the current path if it is shorter.
6. Backtrack by removing the latest vertex and clearing its visited flag.

### Branch-and-bound improvement

If the current partial route is already equal to or longer than the best complete route, that branch is stopped. All road distances are positive, so continuing that branch cannot create a shorter route.

### Complexity

The worst case is exponential because DFS may examine many simple routes. This is acceptable for the controlled 15-location academic map and is honestly documented in the project.

## Clear comparison example

### Request

Airport Terminal to Industrial Zone

### BFS result

```text
Airport Terminal -> Industrial Zone
Stops: 1
Distance: 18 km
```

BFS selects this route because no route can use fewer than one road.

### Recursive DFS result

```text
Airport Terminal -> City Mall -> Industrial Zone
Stops: 2
Distance: 10 km
```

DFS selects this route because its total distance is 8 km shorter, even though it contains an extra road connection.

This example prevents the common incorrect claim that BFS always finds the shortest kilometre distance. BFS guarantees the fewest edges in an unweighted graph.

## Dynamic road handling

The graph can now:

- Block or unblock a road by road ID
- Change both directions of the same road together
- Change traffic level in both directions
- Count blocked roads
- Recalculate BFS and DFS while ignoring blocked edges

The automated test blocks the direct Airport-to-Industrial road and confirms that BFS returns an alternative two-road route.

## Route API

### Request format

```text
GET /api/route?from=5&to=7
```

### Returned sections

- Pickup ID and name
- Destination ID and name
- Minimum Stops route
- Minimum Distance route
- Ordered location path
- Distance
- Estimated time
- Number of road connections
- Visited node count
- Completed DFS route count

## Interface integration

After the customer selects two locations and presses **Compare route options**:

1. JavaScript requests the comparison from C++.
2. Two route cards appear.
3. Minimum Distance is selected initially.
4. The selected path is animated over the SVG map.
5. The user can click Minimum Stops to switch routes.
6. A message explains how many kilometres the DFS route saves.

## Verification completed

Automated tests confirm:

- Dynamic array growth and deep copying
- Queue FIFO ordering and capacity growth
- Stack LIFO ordering
- Graph loading with 15 vertices and 32 roads
- BFS direct route of 18 km
- Recursive DFS minimum route of 10 km
- Correct ordered path for both algorithms
- Blocked-road rerouting
- Same-location route behavior
- Invalid-location rejection
- Route API JSON response
- JavaScript syntax

## Next step

Step 4 will implement the complete parcel workflow and its remaining data structures:

- Singly and doubly linked lists
- Circular rider-notification queue
- Min-heap for parcel priority and rider ranking
- Hash table for tracking IDs
- AVL tree for completed delivery records
- Booking creation and fare calculation
- Careem-style Quick Assign
- inDrive-style Rider Offers
- Parcel status updates and tracking
