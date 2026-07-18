# VS Code Setup on Windows

## Requirements

- VS Code
- Microsoft C/C++ extension
- MSYS2 UCRT64 `g++` and `gdb` on the Windows PATH

The verified user setup uses GCC 16.1.0 and GDB 17.2. Both fully support this C++17 project.

## Easiest method

1. Open the complete `SmartRouteOptimizer` folder in VS Code.
2. Press `Ctrl+Shift+B`.
3. Select **Build Smart Route Optimizer** if VS Code asks.
4. After a successful build, press `F5`.
5. Select **Run Smart Route Optimizer**.
6. Keep the terminal running while using the browser application.

The project includes `.vscode/tasks.json` and `.vscode/launch.json`, so the complete multi-file command is already configured.

## Manual PowerShell method

Run these commands from the folder containing `backend`, `frontend`, and `data`:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
```

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread backend/src/main.cpp backend/src/LocalServer.cpp backend/src/Graph.cpp backend/src/RouteEngine.cpp backend/src/DeliverySystem.cpp backend/src/Security.cpp backend/src/Validation.cpp backend/src/AuthService.cpp backend/src/NetworkManager.cpp backend/src/AnalyticsService.cpp -Ibackend/include -lws2_32 -lshell32 -o build/SmartRouteOptimizer.exe
```

Only run the executable if the compilation command returns without red error text:

```powershell
.\build\SmartRouteOptimizer.exe
```

The dashboard should open automatically at `http://127.0.0.1:8080`.

## MinGW header compatibility

Windows types must be defined before the Shell API is loaded. The corrected project therefore includes the headers in this order:

```cpp
#include <windows.h>
#include <shellapi.h>
```

If compilation fails, the executable is not created. PowerShell will then report that `SmartRouteOptimizer.exe` is not recognized. Always fix the first compiler error before trying to run the program.
