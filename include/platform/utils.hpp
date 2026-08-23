#pragma once
#include <string>

namespace platform {

// Cross-cutting platform utilities (Layer 1).
class PlatformUtils {
    public:
        // Absolute path of the project root ("asset root"): the parent of the
        // current working directory when the app is launched from bin/ (see
        // .vscode/launch.json). Falls back to the current directory when the
        // layout does not match, logging a warning.
        static std::string assetRoot();

        // assetRoot() + '/' + relative (normalized to forward slashes).
        // Absolute `relative` paths are returned unchanged.
        static std::string assetPath(const std::string& relative);
};

}  // namespace platform
