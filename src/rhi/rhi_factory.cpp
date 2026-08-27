#include "rhi/rhi_factory.hpp"

#include <cassert>
#include <stdexcept>

namespace rhi {

vk::raii::ImageView RhiFactory::createImageView(const VmaImage& image,
                                                vk::Format format,
                                                vk::ImageAspectFlags aspectFlags,
                                                std::uint32_t mipLevels) const {
    assert(physicalDevice_ && device_);
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image.getHandle())
            .setFormat(format)
            .setViewType(vk::ImageViewType::e2D)
            .setSubresourceRange({aspectFlags, 0, mipLevels, 0, 1});
    return vk::raii::ImageView(*device_, viewInfo);
}

void RhiFactory::copyBufferToImage(vk::raii::CommandBuffer& cmd,
                                   const VmaBuffer& buffer,
                                   const VmaImage& image,
                                   std::uint32_t width,
                                   std::uint32_t height) const {
    assert(physicalDevice_ && device_);
    vk::BufferImageCopy region;
    vk::ImageSubresourceLayers subresource;
    subresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
               .setBaseArrayLayer(0)
               .setLayerCount(1)
               .setMipLevel(0);
    region.setBufferOffset(0).setBufferImageHeight(0).setBufferRowLength(0)
          .setImageOffset({0, 0, 0})
          .setImageExtent({width, height, 1})
          .setImageSubresource(subresource);

    cmd.copyBufferToImage(buffer.getHandle(), image.getHandle(),
                          vk::ImageLayout::eTransferDstOptimal, region);
}

void RhiFactory::imageBarrier(vk::raii::CommandBuffer& cmd,
                              vk::Image image,
                              vk::ImageLayout oldLayout,
                              vk::ImageLayout newLayout,
                              vk::AccessFlags2 srcAccess,
                              vk::AccessFlags2 dstAccess,
                              vk::PipelineStageFlags2 srcStage,
                              vk::PipelineStageFlags2 dstStage,
                              vk::ImageAspectFlags aspect) const {
    vk::ImageMemoryBarrier2 barrier;
    vk::ImageSubresourceRange range;
    range.setAspectMask(aspect)
         .setBaseMipLevel(0)
         .setLevelCount(1)
         .setBaseArrayLayer(0)
         .setLayerCount(1);
    barrier.setSrcStageMask(srcStage)
           .setSrcAccessMask(srcAccess)
           .setDstStageMask(dstStage)
           .setDstAccessMask(dstAccess)
           .setOldLayout(oldLayout)
           .setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange(range);

    vk::DependencyInfo dependency;
    dependency.setDependencyFlags({})
              .setImageMemoryBarriers(barrier);
    cmd.pipelineBarrier2(dependency);
}

void RhiFactory::transitionImageLayout(vk::raii::CommandBuffer& cmd,
                                       const VmaImage& image,
                                       vk::ImageLayout oldLayout,
                                       vk::ImageLayout newLayout,
                                       std::uint32_t mipLevels) const {
    assert(physicalDevice_ && device_);
    // Ported 1:1 from the legacy implementation — upload semantics unchanged.
    vk::ImageMemoryBarrier barrier;
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setLayerCount(1)
                    .setLevelCount(mipLevels);
    barrier.setOldLayout(oldLayout).setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image.getHandle())
           .setSubresourceRange(subresourceRange);

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;
    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
               newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage      = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    cmd.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

vk::Format RhiFactory::findSupportedFormat(std::span<const vk::Format> candidates,
                                           vk::ImageTiling tiling,
                                           vk::FormatFeatureFlags features) const {
    assert(physicalDevice_ && device_);
    for (const auto format : candidates) {
        const vk::FormatProperties props = physicalDevice_->getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

}  // namespace rhi
