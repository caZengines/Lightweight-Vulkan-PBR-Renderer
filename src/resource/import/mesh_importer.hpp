#pragma once

#include <string>

namespace resource {

class MeshData;

class MeshImporter {
    public:
        // OBJ (.obj) via tinyobjloader.
        static MeshData loadObj(const std::string& path);

        // Dispatch on unknown file extension; fall back to OBJ. glTF files
        // are NOT handled here — they are scenes and must go through
        // GltfSceneImporter::load(); this dispatcher throws on them.
        static MeshData load(const std::string& path);

    private:
};

}  // namespace resource
