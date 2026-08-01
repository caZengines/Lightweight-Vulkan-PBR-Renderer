#pragma once
#include "generic/buffer.hpp"
#include "generic/vertex.hpp"
#include "command_manager.hpp"
#include <string>

class Mesh{
    public:
        //Ban copying
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        explicit Mesh(const std::string modelPath, VmaAllocator* alloc, CommandPool& commandPool);

        const std::vector<Vertex>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

        const VkBuffer& getVertexBuffer() const { return vertexBuffer->getBuffer(); }
        const VkBuffer& getIndexBuffer()  const { return indicesBuffer->getBuffer(); }

    private:
        std::unordered_map<Vertex, uint32_t>     uniqueVertices{};
        std::vector<Vertex>                      vertices;
        std::vector<uint32_t>                    indices;
        
        std::unique_ptr<Buffer<Vertex>>          vertexBuffer = nullptr;
        std::unique_ptr<Buffer<uint32_t>>        indicesBuffer = nullptr;
};