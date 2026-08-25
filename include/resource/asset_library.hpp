#pragma once
#include "resource/resource_registry.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace resource {

// ---------------------------------------------------------------------------
// AssetLibrary — path-keyed asset cache with refcounting (replaces the old
// AssetManager). loadMesh/loadImage return AssetHandle; duplicate loads of
// the same path reuse the cached GPU asset (uploaded once). find* returns a
// null handle when the path was never loaded (Null Object semantics — the
// render layer falls back to the registry's built-in default textures).
//
// Ownership: the library owns the path→id mapping and the per-id reference
// counts. GPU resources are owned by ResourceRegistry and are unloaded when
// the last handle is released. The library must outlive every handle it
// minted (CEngine declares it before the material/scene members).
// ---------------------------------------------------------------------------
class AssetLibrary {
    public:
        explicit AssetLibrary(ResourceRegistry& registry) : registry_(registry) {}
        AssetLibrary(const AssetLibrary&) = delete;
        AssetLibrary& operator=(const AssetLibrary&) = delete;

        // Load (or fetch cached) mesh/image. Throws on import errors.
        AssetHandle loadMesh(const std::string& path);
        AssetHandle loadImage(const std::string& path, vk::Format format, vk::Filter filter);

        // Non-throwing lookups — empty handle when missing.
        AssetHandle findMesh(const std::string& path);
        AssetHandle findImage(const std::string& path);

        // Refcounting (called by AssetHandle).
        void retain(uint32_t id);
        // last reference → unload from the registry
        void release(uint32_t id);

    private:
        std::unordered_map<std::string, uint32_t> paths_;     // path → id
        std::unordered_map<uint32_t, std::string> pathById_;  // id → path (for unload)
        std::unordered_map<uint32_t, uint32_t>    refCounts_; // id → strong count
        ResourceRegistry& registry_;
};

}  // namespace resource
