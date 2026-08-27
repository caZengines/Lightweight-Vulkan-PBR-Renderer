#pragma once

#include <string>

namespace resource {

class MeshData;

// Format-specific CPU importers. Output is pure MeshData (post-processed) —
// GPU upload happens later in ResourceRegistry, so importers never touch
// Vulkan buffers (Phase 2: mesh no longer creates buffers).
class MeshImporter {
    public:
        // OBJ (.obj) via tinyobjloader.
        static MeshData loadObj(const std::string& path);

        // glTF 2.0 (.gltf / .glb) via tinygltf3 — geometry only
        // (POSITION / NORMAL / TEXCOORD_0, sparse accessors, strips/fans).
        static MeshData loadGlTF(const std::string& path);

        // Dispatch on unknown file extension; fall back to OBJ.
        static MeshData load(const std::string& path);

    private:
        static bool hasExtension(const std::string& path, const char* ext);
};

}  // namespace resource
