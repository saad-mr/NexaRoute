#include "LocalServer.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
LocalServer* g_server = nullptr;

void handleSignal(int) {
    if (g_server != nullptr) {
        g_server->stop();
    }
}

std::filesystem::path findProjectRoot() {
    std::filesystem::path candidate = std::filesystem::current_path();

    for (int level = 0; level < 7; ++level) {
        if (std::filesystem::exists(candidate / "frontend" / "index.html") &&
            std::filesystem::exists(candidate / "data" / "locations.csv")) {
            return candidate;
        }

        if (!candidate.has_parent_path()) {
            break;
        }
        candidate = candidate.parent_path();
    }

    return {};
}

void openBrowser(unsigned short port) {
    const std::string url = "http://127.0.0.1:" + std::to_string(port);

#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::cout << "Open " << url << " in your browser.\n";
#endif
}
}

int main(int argc, char* argv[]) {
    unsigned short port = 8080;
    bool launchBrowser = true;
    bool persistenceEnabled = true;
    bool automationEnabled = true;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--no-browser") {
            launchBrowser = false;
        } else if (argument == "--no-persistence") {
            persistenceEnabled = false;
        } else if (argument == "--no-automation") {
            automationEnabled = false;
        } else if (argument == "--port" && index + 1 < argc) {
            const int requestedPort = std::stoi(argv[++index]);
            if (requestedPort < 1024 || requestedPort > 65535) {
                std::cerr << "Port must be between 1024 and 65535.\n";
                return 1;
            }
            port = static_cast<unsigned short>(requestedPort);
        }
    }

    const std::filesystem::path projectRoot = findProjectRoot();
    if (projectRoot.empty()) {
        std::cerr << "Could not locate the frontend and data folders.\n";
        std::cerr << "Run the application from the project or backend folder.\n";
        return 1;
    }

    LocalServer server(projectRoot, port, persistenceEnabled, automationEnabled);
    g_server = &server;
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::cout << "\nNexaRoute C++ Engine\n";
    std::cout << "Project: Smart Route Optimizer\n";
    std::cout << "City: Nexa City\n";
    std::cout << "Address: http://127.0.0.1:" << port << "\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    std::thread browserThread;
    if (launchBrowser) {
        browserThread = std::thread([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(650));
            openBrowser(port);
        });
    }

    const bool started = server.start();

    if (browserThread.joinable()) {
        browserThread.join();
    }

    g_server = nullptr;
    return started ? 0 : 1;
}
