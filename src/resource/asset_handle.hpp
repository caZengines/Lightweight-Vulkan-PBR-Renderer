#pragma once

#include <cstdint>
#include <utility>

namespace resource {

class AssetLibrary;

// ---------------------------------------------------------------------------
// AssetHandle — opaque, refcounted reference to a registry-owned GPU asset.
// A null handle (id == 0) is the Null Object: render code falls back to the
// registry's built-in default assets (GPP Service Locator's Null service).
// Copying a handle retains the asset; the last release unloads it from the
// registry (refcounting lives in AssetLibrary).
//
// Own header (Phase 4): AssetHandle is the CPU-side cross-layer contract, so
// upper layers include this header without pulling in GPU-side declarations.
// ---------------------------------------------------------------------------
class AssetHandle {
    public:
        AssetHandle() noexcept = default;
        AssetHandle(const AssetHandle& other);
        AssetHandle& operator=(const AssetHandle& other);
        AssetHandle(AssetHandle&& other) noexcept;
        AssetHandle& operator=(AssetHandle&& other) noexcept;
        ~AssetHandle();
        void swap(AssetHandle& other) noexcept {
            std::swap(library_, other.library_);
            std::swap(id_, other.id_);
        }

        uint32_t id() const noexcept { return id_; }
        bool     valid() const noexcept { return id_ != 0; }
        explicit operator bool() const noexcept { return valid(); }
        bool operator==(const AssetHandle&) const noexcept = default;

    private:
        friend class AssetLibrary;
        AssetHandle(AssetLibrary* library, uint32_t id) noexcept;

        AssetLibrary* library_ = nullptr;
        uint32_t      id_      = 0;
};

}  // namespace resource
