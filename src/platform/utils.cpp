#include "platform/utils.hpp"

#include "platform/log.hpp"

#include <filesystem>
#include <system_error>

namespace platform {

std::string PlatformUtils::assetRoot() {
    std::error_code   ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        LogLocator::get().write(LogLevel::Warning,
            "PlatformUtils: cannot resolve current working directory, using '.' as asset root");
        return ".";
    }
    // Expected launch layout: <root>/bin  →  asset root is the parent.
    if (cwd.filename() == "bin" && cwd.has_parent_path()) {
        return cwd.parent_path().generic_string();
    }
    LogLocator::get().write(LogLevel::Warning,
        "PlatformUtils: cwd is not <root>/bin (" + cwd.generic_string() +
            "); using cwd as asset root");
    return cwd.generic_string();
}

std::string PlatformUtils::assetPath(const std::string& relative) {
    std::filesystem::path p(relative);
    if (p.is_absolute()) {
        return p.generic_string();
    }
    return (std::filesystem::path(assetRoot()) / p).generic_string();
}

}  // namespace platform
