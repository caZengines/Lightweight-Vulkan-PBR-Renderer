#pragma once

#include <cstdint>
#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "rhi/vma_allocator.hpp"

namespace rhi {

// Stateless GPU helper operations shared by the resource and render layers.
//
// Phase 3 replaces the ResourceFactory singleton: the composition root
// constructs exactly one RhiFactory and threads it down explicitly
// (Core Guidelines / GPP Service Locator: prefer injection over reachable
// globals). Format selection and image barriers were consolidated here from
// their duplicate homes (old ResourceFactory + Renderer/Pipeline copies).
class RhiFactory {
public:
    RhiFactory(vk::raii::PhysicalDevice& physicalDevice,
               vk::raii::Device& device) noexcept
        : physicalDevice_(&physicalDevice), device_(&device) {}

    [[nodiscard("creating a view and dropping it is always a bug")]]
    vk::raii::ImageView createImageView(const VmaImage& image,
                                        vk::Format format,
                                        vk::ImageAspectFlags aspectFlags,
                                        uint32_t mipLevels = 1) const;

    // Upload-path copy (staging buffer → TransferDst-optimized image).
    void copyBufferToImage(vk::raii::CommandBuffer& cmd,
                           const VmaBuffer& buffer,
                           const VmaImage& image,
                           uint32_t width,
                           uint32_t height) const;

    // Single sync2 image-barrier implementation used by command recording.
    void imageBarrier(vk::raii::CommandBuffer& cmd,
                      vk::Image image,
                      vk::ImageLayout oldLayout,
                      vk::ImageLayout newLayout,
                      vk::AccessFlags2 srcAccess,
                      vk::AccessFlags2 dstAccess,
                      vk::PipelineStageFlags2 srcStage,
                      vk::PipelineStageFlags2 dstStage,
                      vk::ImageAspectFlags aspect) const;

    // Legacy sync1 wrapper kept verbatim for the Phase 2 one-shot upload
    // paths (Undefined→TransferDst, TransferDst→ShaderReadOnly).
    void transitionImageLayout(vk::raii::CommandBuffer& cmd,
                               const VmaImage& image,
                               vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout,
                               uint32_t mipLevels) const;

    [[nodiscard]] vk::Format findSupportedFormat(
        std::span<const vk::Format> candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features) const;

    [[nodiscard]] vk::FormatProperties formatProperties(vk::Format format) const {
        return physicalDevice_->getFormatProperties(format);
    }

private:
    // Non-owning; the composition root keeps device/physicalDevice alive.
    vk::raii::PhysicalDevice* physicalDevice_ = nullptr;
    vk::raii::Device*         device_         = nullptr;
};

}  // namespace rhi
