#include "resource/mesh_importer.hpp"
#include "resource/mesh_data.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "extern/tiny_obj_loader.h"

#include <stdexcept>
#include <filesystem>

// =============================================================================
// MeshImporter — CPU-side OBJ import. The single-output format here: one
// file → one post-processed MeshData (deduplication, smooth normal generation,
// tangent computation in MeshData::postProcess).
//
// glTF deliberately does NOT go through this class anymore: a glTF file is a
// scene (one material per primitive, node-transform hierarchy), which does
// not fit the single-MeshData contract. Use resource::GltfSceneImporter and
// wire its primitives into the scene via AssetLibrary::loadMeshData.
// =============================================================================

namespace resource {

MeshData MeshImporter::loadObj(const std::string& modelPath) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str()))
    {
        throw std::runtime_error(err.empty() ? "Failed to load OBJ: " + modelPath : err);
    }
    std::vector<rhi::Vertex>   vertices;
    std::vector<uint32_t> indices;
    size_t cornerCount = 0;
    for (const auto& shape : shapes) cornerCount += shape.mesh.indices.size();
    vertices.reserve(cornerCount);
    indices.reserve(cornerCount);
    const bool hasFileNormals = !attrib.normals.empty();
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            rhi::Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                // OBJ texture V runs bottom-up; flip to match this project's
                // image upload order (row 0 = top of image sampled at v=0).
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }
            vertex.texCoord1 = {0.0f, 0.0f};  // OBJ has no second UV set
            if (hasFileNormals && index.normal_index >= 0) {
                vertex.setNormal(glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                ));
            } else {
                // Missing normals are generated as smooth normals in postProcess
                vertex.setNormal({0.0f, 0.0f, 0.0f});
            }
            vertex.setTangent({0.0f, 0.0f, 0.0f}, 0.0f);  // computed in postProcess
            vertices.emplace_back(vertex);
            indices.emplace_back(static_cast<uint32_t>(indices.size()));
        }
    }
    MeshData data(std::move(vertices), std::move(indices));
    data.postProcess();
    return data;
}

MeshData MeshImporter::load(const std::string& path) {
    std::filesystem::path p(path);
    if (p.extension() == ".gltf" || p.extension() == ".glb") {
        throw std::runtime_error(
            "MeshImporter: '" + path +
            "' is a glTF scene — "
            "load it with resource::GltfSceneImporter::load() instead");
    }
    return loadObj(path);
}

}  // namespace resource
