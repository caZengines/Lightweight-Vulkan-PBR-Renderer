#include "app/app.hpp"
#include "platform/utils.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
    #include <windows.h>
#endif

int runApp() {
    try {
        app::App app{};
        app.run();
    } catch (const std::exception& e) {
        const std::string message = e.what();
        std::cerr << message << std::endl;

        std::ofstream log{platform::PlatformUtils::assetPath("logs/engine.log"),
                          std::ios::app};
        if (log) log << "[ERROR] " << message << std::endl;
#if defined(_WIN32)
        MessageBoxA(nullptr, message.c_str(), "Error", MB_OK | MB_ICONERROR);
#endif
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#if defined(_WIN32)
    int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
        return runApp();
    }
#else
    int main() {
        return runApp();
    }
#endif