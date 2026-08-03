#include "generic/mesh.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "extern/tiny_obj_loader.h"

Mesh::Mesh(const std::string modelPath, VmaAllocator* alloc, CommandPool& commandPool) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str()))
    {
        throw std::runtime_error(err);
    }
    vertices_.reserve(attrib.vertices.size()/3);
    indices_.reserve(attrib.vertices.size());
    bool enableNormal = !attrib.normals.empty();
    std::vector<glm::vec3> oriNormals; oriNormals.reserve(attrib.vertices.size()/3);
    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if(!attrib.texcoords.empty() && index.texcoord_index >= 0){
                vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else {
                vertex.texCoord = {0.0f, 0.0f};
            }
            if(enableNormal){
                vertex.setNormal(glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                ));
            } else {
                vertex.setNormal({0.0f, 0.0f, 0.0f});
            }
            auto [it, inserted] = uniqueVertices_.insert({vertex, static_cast<uint32_t>(vertices_.size())});
            if(inserted){
                vertices_.emplace_back(vertex);
            if (enableNormal) {
                oriNormals.emplace_back(glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                    ));
                } else {
                    oriNormals.emplace_back(0.0f, 0.0f, 0.0f);
                }
            }
            indices_.emplace_back(it->second);
        }
    }
    if(enableNormal && !attrib.texcoords.empty()) {
        std::vector<glm::vec3> tanAccum(vertices_.size());
        std::vector<glm::vec3> bitAccum(vertices_.size());
        std::vector<float>     weightSum(vertices_.size());
        for(size_t f = 0 , nlun = indices_.size()/3; f < nlun ; ++f) {
            uint32_t idx0 = indices_[3 * f + 0];
            uint32_t idx1 = indices_[3 * f + 1];
            uint32_t idx2 = indices_[3 * f + 2];
                
            glm::vec3 deltaPos1 = vertices_[idx1].pos - vertices_[idx0].pos;
            glm::vec3 deltaPos2 = vertices_[idx2].pos - vertices_[idx0].pos;
            glm::vec2 deltaUV1 = vertices_[idx1].texCoord - vertices_[idx0].texCoord;
            glm::vec2 deltaUV2 = vertices_[idx2].texCoord - vertices_[idx0].texCoord;
            float denom = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
            float area  = std::abs(denom);
            if(area > 1e-6f){
                float r = 1.0f / denom;
                glm::vec3 tan = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
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
        for(size_t i = 0, size = vertices_.size() ; i < size ; ++i) {
            if(weightSum[i] < 1e-6f) continue;

            glm::vec3 n = glm::normalize(oriNormals[i]);
            glm::vec3 t = tanAccum[i] / weightSum[i];
            glm::vec3 b = bitAccum[i] / weightSum[i];

            t = glm::normalize(t - n * glm::dot(t, n));
            float handedness = (glm::dot(glm::cross(n, t), b) < 0.0f) ? -1.0f : 1.0f;

            vertices_[i].setTangent(t, handedness);
        }
    } else {
        for(auto& vertex : vertices_) {
            vertex.setNormal({0.0f, 0.0f, 0.0f});
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