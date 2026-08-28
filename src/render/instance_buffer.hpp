#pragma once

#include <cstdint>
#include <vector>

#include "rhi/vertex.hpp"
#include "rhi/vma_allocator.hpp"

namespace resource {
class UploadQueue;
}  // namespace resource

namespace render {

// GPU instance stream of one scene::SceneObject (vertex binding 1: a model
// matrix per instance). Device-local, uploaded once through the resource
// layer's UploadQueue. Transitional shape — the planned ray-tracing pipeline
// replaces per-object streams with TLAS instance transforms.
class InstanceBuffer {
public:
    InstanceBuffer(resource::UploadQueue& queue, const std::vector<rhi::InstanceData>& instances);

    InstanceBuffer(const InstanceBuffer&) = delete;
    InstanceBuffer& operator=(const InstanceBuffer&) = delete;

    [[nodiscard]] VkBuffer handle() const noexcept { return buffer_.getHandle(); }
    [[nodiscard]] uint32_t count() const noexcept { return count_; }

private:
    rhi::VmaBuffer buffer_;
    uint32_t       count_ = 0;
};

}  // namespace render
