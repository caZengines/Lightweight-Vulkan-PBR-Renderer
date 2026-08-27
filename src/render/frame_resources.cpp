#include "render/frame_resources.hpp"

#include "render/frame_uniforms.hpp"
#include "command_manager.hpp"
#include "render_context.hpp"

namespace render {

void FrameResources::init(RenderContext& rct,
                          CommandPool& graphicsPool,
                          VmaAllocator alloc,
                          const vk::DescriptorPool& set0Pool,
                          const vk::DescriptorSetLayout& set0Layout,
                          uint32_t imageCount) {
    device_ = &rct.device;
    createUniformBuffers(alloc);
    createPerFrameSets(set0Pool, set0Layout);
    createCommandBuffers(graphicsPool);
    createSyncObjects(imageCount);
}

void FrameResources::recreateSync(uint32_t newImageCount) {
    // Mirrors the pre-split Renderer::recreateAfterResize / destroySyncObjects
    // pair exactly: counter resets, everything but UBOs/sets/cmds rebuilt.
    device_->waitIdle();
    frameCount_ = 0;
    presentComplete_.clear();
    presentWait_.clear();
    renderTimeline_ = nullptr;
    inFlightFences_.clear();
    createSyncObjects(newImageCount);
}

void FrameResources::createUniformBuffers(VmaAllocator alloc) {
    const vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    vk::BufferCreateInfo bufferCI{};
    bufferCI.setSize(bufferSize)
           .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
           .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT;

    uniformBuffers_.clear();
    uniformBuffers_.reserve(kMaxFramesInFlight);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        uniformBuffers_.emplace_back(alloc, static_cast<const VkBufferCreateInfo&>(bufferCI), allocCI);
    }
}

void FrameResources::createCommandBuffers(CommandPool& graphicsPool) {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(*graphicsPool.setCommandPool())
             .setLevel(vk::CommandBufferLevel::ePrimary)
             .setCommandBufferCount(kMaxFramesInFlight);
    commandBuffers_ = vk::raii::CommandBuffers(*device_, allocInfo);
}

void FrameResources::createSyncObjects(uint32_t imageCount) {
    vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo> timelineChain;
    timelineChain.get<vk::SemaphoreTypeCreateInfo>()
                 .setSemaphoreType(vk::SemaphoreType::eTimeline)
                 .setInitialValue(0);
    renderTimeline_ = vk::raii::Semaphore(*device_, timelineChain.get<vk::SemaphoreCreateInfo>());

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        vk::FenceCreateInfo fenceCI{};
        fenceCI.setFlags(vk::FenceCreateFlagBits::eSignaled);
        inFlightFences_.emplace_back(*device_, fenceCI);
        presentComplete_.emplace_back(*device_, vk::SemaphoreCreateInfo());
    }

    presentWait_.reserve(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        presentWait_.emplace_back(*device_, vk::SemaphoreCreateInfo());
    }
}

void FrameResources::createPerFrameSets(const vk::DescriptorPool& set0Pool,
                                        const vk::DescriptorSetLayout& set0Layout) {
    perFrameSetHandles_.clear();
    std::vector<vk::DescriptorSetLayout> layouts(kMaxFramesInFlight, set0Layout);
    vk::DescriptorSetAllocateInfo alloc{};
    alloc.setDescriptorPool(set0Pool)
         .setDescriptorSetCount(kMaxFramesInFlight)
         .setSetLayouts(layouts);
    perFrameSetObjects_ = vk::raii::DescriptorSets(*device_, alloc);
    perFrameSetHandles_.reserve(perFrameSetObjects_.size());
    for (const auto& set : perFrameSetObjects_) {
        perFrameSetHandles_.emplace_back(*set);
    }
}

}  // namespace render
