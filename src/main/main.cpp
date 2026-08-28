#include "app/app.hpp"
#include <iostream>


int main() {
    try{
        app::App app{};
        app.run();
    } catch (const std::exception& e) {
        std::cerr <<e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
