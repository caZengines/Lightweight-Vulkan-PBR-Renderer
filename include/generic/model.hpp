#pragma once
#include "generic/buffer.hpp"
#include "generic/vertex.hpp"
#include "command_manager.hpp"
#include <string>

class obj_Model{
    public:
        //Ban copying
        obj_Model(const obj_Model&) = delete;
        obj_Model& operator=(const obj_Model&) = delete;

        explicit obj_Model(const std::string modelPath, CommandPool& commandPool);

        const std::vector<Vertex>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

        const vk::raii::Buffer& getVertexBuffer() const { return vertexBuffer->getBuffer(); }
        const vk::raii::Buffer& getIndexBuffer()  const { return indicesBuffer->getBuffer(); }

    private:
        std::unordered_map<Vertex, uint32_t>     uniqueVertices{};
        std::vector<Vertex>                      vertices;
        std::vector<uint32_t>                    indices;
        
        std::unique_ptr<Buffer<Vertex>>          vertexBuffer = nullptr;
        std::unique_ptr<Buffer<uint32_t>>        indicesBuffer = nullptr;
};