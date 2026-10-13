#include "app/app.hpp"
#include <iostream>
#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
#endif

int runApp() {
    try {
        app::App app{};
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
#if defined(_WIN32)
        MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
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