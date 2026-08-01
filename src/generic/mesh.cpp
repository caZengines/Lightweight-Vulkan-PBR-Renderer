#include "generic/mesh.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include "extern/tiny_obj_loader.h"
#include "generic/vertex.hpp"

Mesh::Mesh(const std::string modelPath, VmaAllocator* alloc, CommandPool& commandPool) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str()))
    {
        throw std::runtime_error(err);
    }

    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices){
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            if(!attrib.normals.empty() && index.normal_index >= 0){
                vertex.setNormal({
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                    }
                );
            }
            else {
                vertex.setNormal({0.0f, 0.0f, 0.0f});
            }
            auto [it, inserted] = uniqueVertices.insert({vertex, static_cast<uint32_t>(vertices.size())});
            if(inserted){
                vertices.emplace_back(vertex);
            }
            indices.emplace_back(it->second);
        }
        for(size_t f = 0 ; f < indices.size()/3 ; ++f){
            uint32_t idx0 = indices[3 * f + 0]; const auto& i0 = shape.mesh.indices[3 * f + 0];
            uint32_t idx1 = indices[3 * f + 1]; const auto& i1 = shape.mesh.indices[3 * f + 1];
            uint32_t idx2 = indices[3 * f + 2]; const auto& i2 = shape.mesh.indices[3 * f + 2];
            
            glm::vec3 n0 = {attrib.normals[3 * i0.normal_index + 0], attrib.normals[3 * i0.normal_index + 1], attrib.normals[3 * i0.normal_index + 2]};
            glm::vec3 n1 = {attrib.normals[3 * i1.normal_index + 0], attrib.normals[3 * i1.normal_index + 1], attrib.normals[3 * i1.normal_index + 2]};
            glm::vec3 n2 = {attrib.normals[3 * i2.normal_index + 0], attrib.normals[3 * i2.normal_index + 1], attrib.normals[3 * i2.normal_index + 2]};

            glm::vec3 deltaPos1 = {
                attrib.vertices[3 * i1.vertex_index + 0] - attrib.vertices[3 * i0.vertex_index + 0],
                attrib.vertices[3 * i1.vertex_index + 1] - attrib.vertices[3 * i0.vertex_index + 1],
                attrib.vertices[3 * i1.vertex_index + 2] - attrib.vertices[3 * i0.vertex_index + 2]
            };
            glm::vec3 deltaPos2 = {
                attrib.vertices[3 * i2.vertex_index + 0] - attrib.vertices[3 * i0.vertex_index + 0],
                attrib.vertices[3 * i2.vertex_index + 1] - attrib.vertices[3 * i0.vertex_index + 1],
                attrib.vertices[3 * i2.vertex_index + 2] - attrib.vertices[3 * i0.vertex_index + 2]
            };
            glm::vec2 deltaUV1 = {
                attrib.texcoords[2 * i1.texcoord_index + 0] - attrib.texcoords[2 * i0.texcoord_index + 0],
                attrib.texcoords[2 * i1.texcoord_index + 1] - attrib.texcoords[2 * i0.texcoord_index + 1]
            };
            glm::vec2 deltaUV2 = {
                attrib.texcoords[2 * i2.texcoord_index + 0] - attrib.texcoords[2 * i0.texcoord_index + 0],
                attrib.texcoords[2 * i2.texcoord_index + 1] - attrib.texcoords[2 * i0.texcoord_index + 1]
            };
            vertices[idx0].setTangent(deltaPos1, deltaPos2, deltaUV1, deltaUV2, n0);
            vertices[idx1].setTangent(deltaPos1, deltaPos2, deltaUV1, deltaUV2, n1);
            vertices[idx2].setTangent(deltaPos1, deltaPos2, deltaUV1, deltaUV2, n2);
        }
    }
    Buffer<Vertex>::CreateInfo vertexBufferInfo{};
    vertexBufferInfo.size = sizeof(vertices[0]) * vertices.size();
    vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    vertexBuffer = std::make_unique<Buffer<Vertex>>(alloc, vertices, vertexBufferInfo, commandPool);

    Buffer<uint32_t>::CreateInfo indicesBufferInfo{};
    indicesBufferInfo.size = sizeof(indices[0]) * indices.size();
    indicesBufferInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    indicesBuffer = std::make_unique<Buffer<uint32_t>>(alloc, indices, indicesBufferInfo, commandPool);
}