#pragma once
#include "rhi/vertex.hpp"

#include <cstdint>
#include <vector>

namespace resource {

// host mesh data (Layer 2 discipline: pure data, no Vulkan/GPU types).
// MeshData owns the vertex/index arrays; GPU upload happens later in
// ResourceRegistry — Mesh/MeshGPU no longer create buffers themselves.
class MeshData {
    public:
        MeshData() = default;
        // tangentsFromSource: the vertices already carry source tangents
        // (glTF TANGENT attribute) — postProcess() must not overwrite them.
        // Default false: tangents are computed from TEXCOORD_0.
        MeshData(std::vector<rhi::Vertex> vertices, std::vector<uint32_t> indices,
                 bool tangentsFromSource = false)
            : vertices_(std::move(vertices)), indices_(std::move(indices)),
              tangentsFromSource_(tangentsFromSource) {}

        // Deduplicate vertices by (pos, texcoord, texcoord1, normal, tangent),
        // generate smooth normals when missing, compute tangents (unless
        // tangentsFromSource). Same semantics as the pre-Phase-2 Mesh constructor.
        void postProcess();

        const std::vector<rhi::Vertex>&   vertices() const { return vertices_; }
        const std::vector<uint32_t>& indices()  const { return indices_; }
        bool empty() const { return vertices_.empty() || indices_.empty(); }

    private:
        std::vector<rhi::Vertex>   vertices_;
        std::vector<uint32_t>      indices_;
        bool                  tangentsFromSource_ = false;
};

}  // namespace resource
