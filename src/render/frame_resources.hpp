#pragma once

#include <cstdint>
#include <vector>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "rhi/descriptor_set_allocator.hpp"
#include "rhi/vma_allocator.hpp"

namespace rhi {
class CommandPool;
} //namespace rhi

struct RenderContext;

namespace render {

// Frames allowed to be in flight simultaneously.
inline constexpr uint32_t kMaxFramesInFlight = 1;

// Owns everything that lives for kMaxFramesInFlight rotations: per-frame UBO
// buffers, per-frame Set-0 descriptor sets, command buffers, and all
// synchronization primitives (binary + timeline semaphores, fences).
class FrameResources final {
public:
    FrameResources() = default;
    ~FrameResources();

    // imageCount = current swapchain image count (present-wait semaphores are
    // per-image).  set0Layout/set0Bindings describe the hand-made set 0; the sets
    // themselves come from the app-owned allocator, which must outlive this
    // object (the destructor returns the ids to it).
    void init(RenderContext& rct,
              rhi::CommandPool& graphicsPool,
              VmaAllocator alloc,
              rhi::DescriptorSetAllocator& descriptorSets,
              const vk::DescriptorSetLayout& set0Layout,
              std::span<const rhi::DescriptorBinding> set0Bindings,
              uint32_t imageCount);

    // Swapchain rebuilt: re-create sync objects against the new image count.
    // Uniform buffers / descriptor sets / command buffers survive untouched.
    void recreateSync(uint32_t newImageCount);

    [[nodiscard]] vk::raii::CommandBuffer& commandBuffer(uint32_t frame) { return commandBuffers_[frame]; }
    [[nodiscard]] const vk::raii::Fence&     inFlightFence(uint32_t frame) const { return inFlightFences_[frame]; }
    [[nodiscard]] const vk::raii::Semaphore& presentComplete(uint32_t frame) const { return presentComplete_[frame]; }
    [[nodiscard]] const vk::raii::Semaphore& presentWait(uint32_t imageIndex) const { return presentWait_[imageIndex]; }
    [[nodiscard]] const vk::raii::Semaphore& renderTimeline() const { return renderTimeline_; }

    [[nodiscard]] rhi::VmaBuffer& uniformBuffer(uint32_t frame) { return uniformBuffers_[frame]; }
    [[nodiscard]] const std::vector<vk::DescriptorSet>& descriptorSetHandles() const { return perFrameSetHandles_; }

    // Monotonic timeline payload; resets on sync recreation (same behavior as
    // the pre-split Renderer, which zeroed frameCount on rebuild).
    [[nodiscard]] uint64_t nextSignalValue() { return ++frameCount_; }

private:
    void createUniformBuffers(VmaAllocator alloc);
    void createCommandBuffers(rhi::CommandPool& graphicsPool);
    void createSyncObjects(uint32_t imageCount);
    void createPerFrameSets(rhi::DescriptorSetAllocator& descriptorSets,
                            const vk::DescriptorSetLayout& set0Layout,
                            std::span<const rhi::DescriptorBinding> set0Bindings);

    vk::raii::Device*                     device_ = nullptr;  // non-owning, set by init()
    rhi::DescriptorSetAllocator*          descriptorSets_ = nullptr;  // non-owning

    std::vector<rhi::VmaBuffer>           uniformBuffers_;
    std::vector<rhi::DescriptorSetId>     perFrameSetIds_;
    std::vector<vk::DescriptorSet>        perFrameSetHandles_;
    std::vector<vk::raii::CommandBuffer>  commandBuffers_;

    std::vector<vk::raii::Semaphore>      presentComplete_;               // per frame-in-flight
    std::vector<vk::raii::Semaphore>      presentWait_;                   // per swapchain image
    vk::raii::Semaphore                   renderTimeline_      = nullptr; // signal = ++frameCount_
    std::vector<vk::raii::Fence>          inFlightFences_;

    uint64_t                              frameCount_ = 0;
};

}  // namespace render
