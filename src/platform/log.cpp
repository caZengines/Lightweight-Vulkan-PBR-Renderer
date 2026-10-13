#include "platform/log.hpp"

#include <filesystem>
#include <iostream>
#include <mutex>

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

const char* levelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "?";
}

} // namespace

// ---------------------------------------------------------------------------
// ConsoleLog
// ---------------------------------------------------------------------------
void ConsoleLog::write(LogLevel level, const std::string& message) {
    static const bool configured = [] {
        std::ios_base::sync_with_stdio(false);
        std::cout.tie(nullptr);
        std::cerr.tie(nullptr);
        return true;
    }();
    std::ignore = configured;
    if (level == LogLevel::Error) {
        std::cerr << message << '\n';
        std::cerr.flush();
    } else {
        std::cout << message<< '\n';
    }
}

// ---------------------------------------------------------------------------
// FileLog
// ---------------------------------------------------------------------------
namespace fs = std::filesystem;
FileLog::FileLog(const fs::path& path, bool truncate) {
    std::error_code ec;
    if(path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }

    std::ios_base::openmode mode = std::ios::out | std::ios::binary;
    mode |= truncate ? std::ios::trunc : std::ios::app;

    file_.open(path, mode);
    static thread_local char buffer[64 * 1024];
    if (file_.is_open()) {
        file_.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    }
}
FileLog::~FileLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void FileLog::write(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) return;

    file_ << '[' << levelTag(level) << "] " << message << '\n';
    if (level == LogLevel::Error) {
        file_.flush();
    }
}

void FileLog::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.flush();
}

// ---------------------------------------------------------------------------
// LogLocator
// ---------------------------------------------------------------------------
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
