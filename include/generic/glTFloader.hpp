#pragma once
#include "generic/mesh.hpp"
#include "command_manager.hpp"
#include <memory>
#include <string>

// glTF 2.0 model loaded from a .gltf / .glb file.
// Loads triangle geometry (POSITION, TEXCOORD_0, NORMAL); missing normals are
// regenerated as smooth normals and tangents are computed by Mesh.
class glTFModel {
    public:
        //ban copying
        glTFModel(const glTFModel&) = delete;
        glTFModel& operator=(const glTFModel&) = delete;

        static std::unique_ptr<glTFModel> fromglTF(const std::string& modelPath,
                                                   VmaAllocator* alloc, CommandPool& commandPool);

        const Mesh& getMesh() const { return *mesh_; }
        std::shared_ptr<const Mesh> getMeshShared() const { return mesh_; }

    private:
        explicit glTFModel(std::shared_ptr<const Mesh> mesh) : mesh_(std::move(mesh)) {}

        std::shared_ptr<const Mesh> mesh_ = nullptr;
};
