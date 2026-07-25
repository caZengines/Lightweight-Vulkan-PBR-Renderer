#include "resourcefactory.hpp"
#include "vulkan/vulkan.hpp"

std::pair<vk::raii::Image, vk::raii::DeviceMemory> ResourceFactory::createImage(uint32_t width, uint32_t height, uint32_t _mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::SharingMode mode, const std::vector<uint32_t>& queueFamilies) const {
    assert(physicalDevice && device);
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1})
             .setFormat(format)
             .setTiling(tiling)
             .setUsage(usage)
             .setMipLevels(_mipLevels)
             .setArrayLayers(1)
             .setImageType(vk::ImageType::e2D)
             .setSamples(numSamples);
    if(mode == vk::SharingMode::eConcurrent){
        imageInfo.setQueueFamilyIndices(queueFamilies);
    }
    vk::raii::Image         image = vk::raii::Image(*device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo memAllocateInfo;
    memAllocateInfo.setAllocationSize(memRequirements.size)
                   .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
    vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(*device, memAllocateInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
}

vk::raii::ImageView ResourceFactory::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels_) const {
    assert(physicalDevice && device);
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image)
            .setFormat(format)
            .setViewType(vk::ImageViewType::e2D)
            .setSubresourceRange({aspectFlags, 0, mipLevels_, 0, 1});
    return vk::raii::ImageView(*device, viewInfo);
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> ResourceFactory::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::SharingMode mode, const std::vector<uint32_t>& queueFamilyIndices) const {
    assert(physicalDevice && device);
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(size).setUsage(usage).setSharingMode(mode);
    if(mode == vk::SharingMode::eConcurrent){
        bufferInfo.setQueueFamilyIndices(queueFamilyIndices);
    }
    vk::raii::Buffer        buffer = vk::raii::Buffer(*device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memAllocateInfo;
    memAllocateInfo.setAllocationSize(memRequirements.size)
                   .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
    vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(*device, memAllocateInfo);
    buffer.bindMemory(bufferMemory, 0);

    return {std::move(buffer), std::move(bufferMemory)};
}

void ResourceFactory::copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height) const {
    assert(physicalDevice && device);
    vk::BufferImageCopy region;
    vk::ImageSubresourceLayers imageSubresource;imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor).setBaseArrayLayer(0).setLayerCount(1).setMipLevel(0);
    region.setBufferOffset(0).setBufferImageHeight(0).setBufferRowLength(0)
          .setImageOffset({0,0,0})
          .setImageExtent({width, height, 1})
          .setImageSubresource(imageSubresource);

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

void ResourceFactory::transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels_) const {
    vk::ImageMemoryBarrier barrier;
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1).setLevelCount(mipLevels_);
    barrier.setOldLayout(oldLayout).setNewLayout(newLayout)
           .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored).setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image).setSubresourceRange(subresourceRange);
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

uint32_t ResourceFactory::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    assert(physicalDevice && device);
    vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice->getMemoryProperties();
    for(int i = 0 ; i < memoryProperties.memoryTypeCount ;++i){
        if(( typeFilter & (1 << i) ) && ( (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) ){
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format ResourceFactory::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features){
    assert(physicalDevice && device);
    for(const auto& format : candidates){
        vk::FormatProperties props = physicalDevice->getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features){
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features){
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format! ");
}