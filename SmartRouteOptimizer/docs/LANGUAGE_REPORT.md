# Executable Source Language Report

## Result

The project meets the agreed minimum of 80% C++ executable source by line count.

| Executable source | Lines | Share |
|---|---:|---:|
| C++17 (`.cpp` and `.h` in `backend` and `tests`) | 7,741 | **80.3%** |
| Browser behavior (`frontend/app.js`) | 1,904 | **19.7%** |
| Total executable source | 9,645 | 100.0% |

HTML and CSS are interface resources, not executable business logic, so they are intentionally excluded from this measurement. C++ unit and integration tests are included because they are compiled executable C++ source that verifies the backend. The production backend alone contains 6,481 C++ lines and represents 77.3% when compared directly with `app.js`.

## What C++ owns

- Graph storage and dynamic graph expansion
- Minimum Stops and Minimum Distance route calculation
- Parcel validation, fare calculation, rider matching, and offers
- Booking lifecycle, automatic status scheduling, and tracking
- Empty initial rider state, starting/current-location validation, and genuine customer rating averages
- Rider/admin credentials, salted password hashing, account locks, and sessions
- Role authorization and private-data filtering
- Named destination and named route validation/persistence
- Road blocking, traffic changes, route recalculation, and undo state
- Fleet, finance, traffic, reachability, and leaderboard analytics
- All runtime persistence and restore logic
- Native local HTTP server and JSON responses

JavaScript is limited to browser interaction: sending requests, rendering returned data, map animation, modal behavior, notifications, and responsive workspace controls.

## Reproduce the count

From a Bash terminal in the project root:

```bash
CPP=$(find backend tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 cat | wc -l)
JS=$(wc -l < frontend/app.js)
awk -v cpp="$CPP" -v js="$JS" 'BEGIN { printf "C++ %.1f%%, JavaScript %.1f%%\n", 100*cpp/(cpp+js), 100*js/(cpp+js) }'
```

The measurement is based on source lines, not file count or filename extensions disguised as another language.
