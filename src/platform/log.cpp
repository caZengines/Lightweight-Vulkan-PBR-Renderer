#include "platform/log.hpp"

#include <iostream>

namespace platform {

namespace {

// Function-local statics avoid static initialization order issues.
Log*& serviceRef() {
    static Log* service = nullptr;
    return service;
}

NullLog& nullLog() {
    static NullLog instance;
    return instance;
}

ConsoleLog& consoleLog() {
    static ConsoleLog instance;
    return instance;
}

} // namespace

void ConsoleLog::write(LogLevel level, const std::string& message) {
    std::ios_base::sync_with_stdio(false); std::cout.tie(nullptr);
    if (level == LogLevel::Error) {
        std::cerr << message << std::endl;
    } else {
        std::cout << message<< std::endl;
    }
}

void LogLocator::initialize() {
    serviceRef() = &consoleLog();
}

Log& LogLocator::get() {
    Log* service = serviceRef();
    return service ? *service : nullLog();
}

void LogLocator::provide(Log* service) {
    serviceRef() = service ? service : &nullLog();
}

} // namespace platform
