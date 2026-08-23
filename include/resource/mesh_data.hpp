#pragma once
#include "generic/vertex.hpp"

#include <cstdint>
#include <vector>

namespace resource {

// CPU-side mesh data (Layer 2 discipline: pure data, no Vulkan/GPU types).
// MeshData owns the vertex/index arrays; GPU upload happens later in
// ResourceRegistry — Mesh/MeshGPU no longer create buffers themselves.
class MeshData {
    public:
        MeshData() = default;
        MeshData(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
            : vertices_(std::move(vertices)), indices_(std::move(indices)) {}

        // Deduplicate vertices by (pos, texcoord, normal), generate smooth
        // normals when missing, compute tangents. Same semantics as the
        // pre-Phase-2 Mesh constructor.
        void postProcess();

        const std::vector<Vertex>&   vertices() const { return vertices_; }
        const std::vector<uint32_t>& indices()  const { return indices_; }
        bool empty() const { return vertices_.empty() || indices_.empty(); }

    private:
        std::vector<Vertex>   vertices_;
        std::vector<uint32_t> indices_;
};

}  // namespace resource
