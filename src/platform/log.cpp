#include "platform/log.hpp"

#include <atomic>
#include <filesystem>
#include <ios>
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

thread_local FileLog::ThreadBuf FileLog::tls_{};

FileLog::ThreadBuf::~ThreadBuf() {
    if(owner && !pending.empty()) {
        owner->flushRaw(pending.data(), pending.size());
    }
}

FileLog::FileLog(const fs::path& path, bool truncate) {
    std::error_code ec;
    if(path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }

    std::ios_base::openmode mode = std::ios::out | std::ios::binary;
    mode |= truncate ? std::ios::trunc : std::ios::app;

    file_.open(path, mode);
}
FileLog::~FileLog() {
    alive_.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lock(mutex_);
    if(file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void FileLog::write(LogLevel level, const std::string& message) {
    auto& buf = tls_;
    if(buf.owner != this) {
        if (buf.owner && !buf.pending.empty()) {
            buf.owner->flushRaw(buf.pending.data(), buf.pending.size());
        }
        buf.owner = this;
        buf.pending.clear();
        buf.lines = 0;
        buf.hasError = false;
    }
    buf.pending.reserve(buf.pending.size() + message.size() + 16);
    buf.pending += '[';
    buf.pending += levelTag(level);
    buf.pending += "] ";
    buf.pending += message;
    buf.pending += '\n';
    ++buf.lines;
    buf.hasError |= (level == LogLevel::Error);

    if(buf.lines > kFlushLines ||
       buf.pending.size() > kFlushBytes ||
       level == LogLevel::Error) {
            drain(buf);
       }
}

void FileLog::flush() {
    if(tls_.owner == this && !tls_.pending.empty()) {
        drain(tls_);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if(file_.is_open()) file_.flush();
}

void FileLog::drain(ThreadBuf &buf) {
    if(buf.pending.empty()) return;

    const bool doFlush = buf.hasError;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(alive_.load(std::memory_order_acquire) && file_.is_open()) {
            file_.write(buf.pending.data(), 
                        static_cast<std::streamsize>(buf.pending.size()));
            if(doFlush) file_.flush();
        }
    }
    buf.pending.clear();
    buf.lines = 0;
    buf.hasError = false;
}

void FileLog::flushRaw(const char* data, size_t n) {
    if (!alive_.load(std::memory_order_acquire)) return;
    std::lock_guard lock(mutex_);
    if (file_.is_open()) file_.write(data, static_cast<std::streamsize>(n));
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
