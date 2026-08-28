#include "render/instance_buffer.hpp"

#include "resource/upload_queue.hpp"

namespace render {

InstanceBuffer::InstanceBuffer(resource::UploadQueue& queue, const std::vector<rhi::InstanceData>& instances)
    : count_(static_cast<uint32_t>(instances.size()))
{
    buffer_ = queue.uploadBuffer(instances.data(),
                                 sizeof(rhi::InstanceData) * instances.size(),
                                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);
}

}  // namespace render
