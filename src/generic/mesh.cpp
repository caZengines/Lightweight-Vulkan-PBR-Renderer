#include "generic/mesh.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "extern/tiny_obj_loader.h"
#include <unordered_map>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices,
           VmaAllocator* alloc, CommandPool& commandPool) {
    // Deduplicate vertices by (pos, texcoord, normal)
    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    uniqueVertices.reserve(vertices.size());
    std::vector<Vertex>   deduped;
    std::vector<uint32_t> dedupedIndices;
    deduped.reserve(vertices.size());
    dedupedIndices.reserve(indices.size());
    for (const uint32_t& index : indices) {
        auto [it, inserted] = uniqueVertices.insert({vertices[index], static_cast<uint32_t>(deduped.size())});
        if (inserted) {
            deduped.emplace_back(vertices[index]);
        }
        dedupedIndices.emplace_back(it->second);
    }
    vertices_ = std::move(deduped);
    indices_  = std::move(dedupedIndices);

    bool hasNormals     = false;
    bool hasTexCoords   = false;
    bool missingNormals = false;
    for (const Vertex& v : vertices_) {
        if ((v.normal[0] | v.normal[1] | v.normal[2]) != 0) {
            hasNormals = true;
        } else {
            missingNormals = true;
        }
        if (v.texCoord.x != 0.0f || v.texCoord.y != 0.0f) hasTexCoords = true;
    }

    // Models without normal data (or with partially missing normals) get smooth,
    // area-weighted normals generated from the deduplicated index list.
    if (missingNormals) {
        std::vector<glm::vec3> accum(vertices_.size());
        for (size_t f = 0; f + 2 < indices_.size(); f += 3) {
            uint32_t i0 = indices_[f + 0];
            uint32_t i1 = indices_[f + 1];
            uint32_t i2 = indices_[f + 2];

            glm::vec3 faceNormal = glm::cross(vertices_[i1].pos - vertices_[i0].pos,
                                              vertices_[i2].pos - vertices_[i0].pos);
            if (glm::dot(faceNormal, faceNormal) < 1e-12f) continue; // degenerate face
            accum[i0] += faceNormal;
            accum[i1] += faceNormal;
            accum[i2] += faceNormal;
        }
        for (size_t i = 0, size = vertices_.size(); i < size; ++i) {
            if ((vertices_[i].normal[0] | vertices_[i].normal[1] | vertices_[i].normal[2]) != 0) continue;
            if (glm::dot(accum[i], accum[i]) < 1e-12f) continue; // fully degenerate vertex, leave zero
            vertices_[i].setNormal(glm::normalize(accum[i]));
            hasNormals = true;
        }
    }

    if (hasNormals && hasTexCoords) {
        std::vector<glm::vec3> tanAccum(vertices_.size());
        std::vector<glm::vec3> bitAccum(vertices_.size());
        std::vector<float>     weightSum(vertices_.size());
        for (size_t f = 0, numTriangles = indices_.size() / 3; f < numTriangles; ++f) {
            uint32_t idx0 = indices_[3 * f + 0];
            uint32_t idx1 = indices_[3 * f + 1];
            uint32_t idx2 = indices_[3 * f + 2];

            glm::vec3 deltaPos1 = vertices_[idx1].pos - vertices_[idx0].pos;
            glm::vec3 deltaPos2 = vertices_[idx2].pos - vertices_[idx0].pos;
            glm::vec2 deltaUV1 = vertices_[idx1].texCoord - vertices_[idx0].texCoord;
            glm::vec2 deltaUV2 = vertices_[idx2].texCoord - vertices_[idx0].texCoord;
            float denom = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
            float area  = std::abs(denom);
            if (area > 1e-6f) {
                float r = 1.0f / denom;
                glm::vec3 tan   = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
                glm::vec3 bitan = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

                tanAccum[idx0]  += area * tan;
                bitAccum[idx0]  += area * bitan;
                weightSum[idx0] += area;
                tanAccum[idx1]  += area * tan;
                bitAccum[idx1]  += area * bitan;
                weightSum[idx1] += area;
                tanAccum[idx2]  += area * tan;
                bitAccum[idx2]  += area * bitan;
                weightSum[idx2] += area;
            }
        }
        for (size_t i = 0, size = vertices_.size(); i < size; ++i) {
            if (weightSum[i] < 1e-6f) {
                // Degenerate UVs: fall back to an arbitrary tangent so the
                // shader never sees a zero-length tangent basis.
                vertices_[i].setTangent({1.0f, 0.0f, 0.0f}, 1.0f);
                continue;
            }

            // Normal is stored SNORM-quantized; decode it back for the tangent basis
            glm::vec3 decoded = glm::vec3(vertices_[i].normal[0],
                                          vertices_[i].normal[1],
                                          vertices_[i].normal[2]) / 127.0f;
            float normalLen = glm::length(decoded);
            if (normalLen < 1e-6f) {
                vertices_[i].setTangent({1.0f, 0.0f, 0.0f}, 1.0f);
                continue;
            }
            glm::vec3 n = decoded / normalLen;
            glm::vec3 t = tanAccum[i] / weightSum[i];
            glm::vec3 b = bitAccum[i] / weightSum[i];

            t = glm::normalize(t - n * glm::dot(t, n));
            float handedness = (glm::dot(glm::cross(n, t), b) < 0.0f) ? -1.0f : 1.0f;

            vertices_[i].setTangent(t, handedness);
        }
    } else {
        for (auto& vertex : vertices_) {
            vertex.setTangent({1.0f, 0.0f, 0.0f}, 1.0f);
        }
    }

    Buffer<Vertex>::CreateInfo vertexBufferInfo{};
    vertexBufferInfo.size = sizeof(vertices_[0]) * vertices_.size();
    vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    vertexBuffer = std::make_unique<Buffer<Vertex>>(alloc, vertices_, vertexBufferInfo, commandPool);

    Buffer<uint32_t>::CreateInfo indicesBufferInfo{};
    indicesBufferInfo.size = sizeof(indices_[0]) * indices_.size();
    indicesBufferInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    indicesBuffer = std::make_unique<Buffer<uint32_t>>(alloc, indices_, indicesBufferInfo, commandPool);
}

std::unique_ptr<Mesh> Mesh::fromObj(const std::string& modelPath,
                                    VmaAllocator* alloc, CommandPool& commandPool) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str()))
    {
        throw std::runtime_error(err.empty() ? "Failed to load OBJ: " + modelPath : err);
    }
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    size_t cornerCount = 0;
    for (const auto& shape : shapes) cornerCount += shape.mesh.indices.size();
    vertices.reserve(cornerCount);
    indices.reserve(cornerCount);
    const bool hasFileNormals = !attrib.normals.empty();
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }
            if (hasFileNormals && index.normal_index >= 0) {
                vertex.setNormal(glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                ));
            } else {
                // Missing normals are generated as smooth normals in the Mesh constructor
                vertex.setNormal({0.0f, 0.0f, 0.0f});
            }
            vertices.emplace_back(vertex);
            indices.emplace_back(static_cast<uint32_t>(indices.size()));
        }
    }
    return std::make_unique<Mesh>(std::move(vertices), std::move(indices), alloc, commandPool);
}
