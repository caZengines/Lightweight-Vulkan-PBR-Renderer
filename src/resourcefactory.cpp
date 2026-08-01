#include "resourcefactory.hpp"

vk::raii::ImageView ResourceFactory::createImageView(const VmaImage& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels_) const {
    assert(physicalDevice_ && device_);
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image.getHandle())
            .setFormat(format)
            .setViewType(vk::ImageViewType::e2D)
            .setSubresourceRange({aspectFlags, 0, mipLevels_, 0, 1});
    return vk::raii::ImageView(*device_, viewInfo);
}

void ResourceFactory::copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const VmaBuffer &buffer, const VmaImage &image, uint32_t width, uint32_t height) const {
    assert(physicalDevice_ && device_);
    vk::BufferImageCopy region;
    vk::ImageSubresourceLayers imageSubresource;imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor).setBaseArrayLayer(0).setLayerCount(1).setMipLevel(0);
    region.setBufferOffset(0).setBufferImageHeight(0).setBufferRowLength(0)
          .setImageOffset({0,0,0})
          .setImageExtent({width, height, 1})
          .setImageSubresource(imageSubresource);

    commandBuffer.copyBufferToImage(buffer.getHandle(), image.getHandle(), vk::ImageLayout::eTransferDstOptimal, region);
}

void ResourceFactory::transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const VmaImage &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels_) const {
    vk::ImageMemoryBarrier barrier;
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(mipLevels_);
    barrier.setOldLayout(oldLayout).setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image.getHandle()).setSubresourceRange(subresourceRange);
    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal){
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal){
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite; //wait for "transferWrite opearation" to complete.
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead; // make the shader able to read visible data.

        sourceStage      = vk::PipelineStageFlagBits::eTransfer; //wait for transfer stage to complete all access.
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader; //if the transfer stage completes, the process can move to the fragment shader stage.
    }
    else{
        throw std::invalid_argument("unsupported layout transition!");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

vk::Format ResourceFactory::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features){
    assert(physicalDevice_ && device_);
    for(const auto& format : candidates){
        vk::FormatProperties props = physicalDevice_->getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features){
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features){
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format! ");
}