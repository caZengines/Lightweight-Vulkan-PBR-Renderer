#include "render/frame_resources.hpp"

#include "render/frame_uniforms.hpp"
#include "rhi/command_pool.hpp"
#include "render_context.hpp"

namespace render {

FrameResources::~FrameResources() {
    if (descriptorSets_ == nullptr) return;
    for (const auto id : perFrameSetIds_) {
        if (descriptorSets_->alive(id)) descriptorSets_->release(id);
    }
    perFrameSetIds_.clear();
    perFrameSetHandles_.clear();
}

void FrameResources::init(RenderContext& rct,
                          rhi::CommandPool& graphicsPool,
                          VmaAllocator alloc,
                          rhi::DescriptorSetAllocator& descriptorSets,
                          const vk::DescriptorSetLayout& set0Layout,
                          std::span<const rhi::DescriptorBinding> set0Bindings,
                          uint32_t imageCount) {
    device_          = &rct.device;
    descriptorSets_  = &descriptorSets;
    createUniformBuffers(alloc);
    createPerFrameSets(descriptorSets, set0Layout, set0Bindings);
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

void FrameResources::createCommandBuffers(rhi::CommandPool& graphicsPool) {
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

void FrameResources::createPerFrameSets(rhi::DescriptorSetAllocator& descriptorSets,
                                        const vk::DescriptorSetLayout& set0Layout,
                                        std::span<const rhi::DescriptorBinding> set0Bindings) {
    const auto allocated = descriptorSets.allocate(set0Layout, set0Bindings, kMaxFramesInFlight);

    perFrameSetIds_.clear();
    perFrameSetHandles_.clear();
    perFrameSetIds_.reserve(allocated.size());
    perFrameSetHandles_.reserve(allocated.size());
    for (const auto& entry : allocated) {
        perFrameSetIds_.emplace_back(entry.id);
        perFrameSetHandles_.emplace_back(entry.set);
    }
}

}  // namespace render
