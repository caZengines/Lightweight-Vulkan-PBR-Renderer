#pragma once
// ============================================================================
// Platform Abstraction Layer: Log (Service Locator pattern)
//
// The engine's single logging service. Consumers depend on the abstract Log
// interface; LogLocator::provide() lets the composition root swap providers
// (console / null / decorated) without touching call sites.
//
//   LogLocator::initialize();                 // default: ConsoleLog
//   LogLocator::provide(nullptr);             // revert to NullLog (silence)
//   LogLocator::get().write(LogLevel::Info, "hello");
//
// The locator is guaranteed to never return null (Null Object).
// ============================================================================

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace platform {

enum class LogLevel : uint8_t { Info, Warning, Error };

class Log {
public:
    virtual ~Log() = default;
    virtual void write(LogLevel level, const std::string& message) = 0;
    virtual void flush() {}
};

class ConsoleLog final : public Log {
public:
    void write(LogLevel level, const std::string& message) override;
};

class FileLog final : public Log {
public:
    explicit FileLog(const std::filesystem::path& path, bool truncate = true);
    ~FileLog() override;
    
    FileLog(const FileLog&) = delete;
    FileLog& operator=(const FileLog&) = delete;;

    void write(LogLevel level, const std::string& message) override;
    void flush() override;

    bool isOpen() const { return file_.is_open(); }
private:
    std::ofstream file_;
    std::mutex    mutex_;
};

class NullLog final : public Log {
public:
    void write(LogLevel, const std::string&) override {}
};

class LogLocator {
public:
    // Sets the default provider (ConsoleLog). Safe to call more than once.
    static void initialize();

    // Never returns null — falls back to NullLog before initialize().
    static Log& get();

    // Replace the active provider. nullptr reverts to the NullLog provider.
    static void provide(Log* service);

private:
    LogLocator() = delete;
};

} // namespace platform
