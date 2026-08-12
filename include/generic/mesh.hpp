#pragma once
#include "generic/buffer.hpp"
#include "generic/vertex.hpp"
#include "command_manager.hpp"
#include <string>

// Mesh accepts raw vector without the need for deduplication.
class Mesh{
    public:
        // Ban copying
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        // Format-agnostic: deduplicates vertices, generates smooth normals if missing,
        // computes tangents, uploads to GPU
        Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices,
             VmaAllocator* alloc, CommandPool& commandPool);

        // Default OBJ-specific loading, without specified format
        static std::unique_ptr<Mesh> fromObj(const std::string& modelPath,
                                             VmaAllocator* alloc, CommandPool& commandPool);

        const std::vector<Vertex>& getVertices() const { return vertices_; }
        const std::vector<uint32_t>& getIndices() const { return indices_; }

        const VkBuffer& getVertexBuffer() const { return vertexBuffer->getBuffer(); }
        const VkBuffer& getIndexBuffer()  const { return indicesBuffer->getBuffer(); }

    private:
        std::vector<Vertex>                      vertices_;
        std::vector<uint32_t>                    indices_;

        std::unique_ptr<Buffer<Vertex>>          vertexBuffer = nullptr;
        std::unique_ptr<Buffer<uint32_t>>        indicesBuffer = nullptr;
};
