#include "resource/upload_queue.hpp"

#include "rhi/rhi_factory.hpp"

#include <cstring>
#include <stdexcept>

namespace resource {

rhi::VmaBuffer UploadQueue::uploadBuffer(const void* data,
                                    vk::DeviceSize size, vk::BufferUsageFlags usage) {
    // --- Host-visible staging buffer ---
    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.setSize(size).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    VmaAllocationCreateInfo stagingCI{};
    stagingCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    stagingCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    rhi::VmaBuffer stagingBuffer(allocator_, static_cast<const VkBufferCreateInfo&>(stagingInfo), stagingCI);

    void* dataStaging = stagingBuffer.map();
    std::memcpy(dataStaging, data, size);
    stagingBuffer.unmap();

    // --- Device-local destination buffer ---
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setSize(size).setUsage(usage).setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO;
    rhi::VmaBuffer deviceBuffer(allocator_, static_cast<const VkBufferCreateInfo&>(bufferInfo), allocCI);

    vk::raii::CommandBuffer commandBuffer = pool_.beginSingleTimeCommands();
    commandBuffer.copyBuffer(stagingBuffer.getHandle(), deviceBuffer.getHandle(),
                             vk::BufferCopy(0, 0, size));
    pool_.endSingleTimeCommands(std::move(commandBuffer));
    return deviceBuffer;
}

void UploadQueue::uploadImage(const void* data, vk::DeviceSize size,
                              rhi::VmaImage& image, uint32_t width, uint32_t height,
                              uint32_t mipLevels, vk::Format format, vk::Filter filter) {
    // --- Host-visible staging buffer ---
    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.setSize(size).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    VmaAllocationCreateInfo stagingCI{};
    stagingCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    stagingCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    rhi::VmaBuffer stagingBuffer(allocator_, static_cast<const VkBufferCreateInfo&>(stagingInfo), stagingCI);

    void* dataStaging = stagingBuffer.map();
    std::memcpy(dataStaging, data, size);
    stagingBuffer.unmap();

    // --- Undefined → TransferDst, copy base mip ---
    vk::raii::CommandBuffer commandBuffer = pool_.beginSingleTimeCommands();
    factory_.transitionImageLayout(commandBuffer, image,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal, mipLevels);
    factory_.copyBufferToImage(commandBuffer, stagingBuffer, image, width, height);
    pool_.endSingleTimeCommands(std::move(commandBuffer));

    // --- Mip chain (transitions to ShaderReadOnly along the way), or a plain
    //     TransferDst → ShaderReadOnly transition for single-mip images ---
    if (mipLevels > 1) {
        generateMipmaps(image, format, filter, width, height, mipLevels);
    } else {
        vk::raii::CommandBuffer finalTransition = pool_.beginSingleTimeCommands();
        factory_.transitionImageLayout(finalTransition, image,
                                      vk::ImageLayout::eTransferDstOptimal,
                                      vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);
        pool_.endSingleTimeCommands(std::move(finalTransition));
    }
}

void UploadQueue::generateMipmaps(rhi::VmaImage& image, vk::Format imageFormat, vk::Filter filter_,
                                  uint32_t texWidth_, uint32_t texHeight_,
                                  uint32_t mipLevels_) {
    const vk::FormatProperties formatProperties = factory_.formatProperties(imageFormat);
    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::raii::CommandBuffer commandBuffer = pool_.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier;
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eTransferRead)
           .setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
           .setDstQueueFamilyIndex(vk::QueueFamilyIgnored).setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image.getHandle());
    barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor).setBaseArrayLayer(0).setLayerCount(1).setLevelCount(1);

    uint32_t mipWidth  = texWidth_;
    uint32_t mipHeight = texHeight_;
    for (uint32_t i = 1; i < mipLevels_; ++i) {
        barrier.subresourceRange.setBaseMipLevel(i - 1);
        barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eTransferRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
        offsets[0] = vk::Offset3D(0, 0, 0);
        offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
        dstOffsets[0] = vk::Offset3D(0, 0, 0);
        dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);
        vk::ImageBlit blit;
        blit.setSrcSubresource({vk::ImageAspectFlagBits::eColor, i - 1, 0, 1}).setSrcOffsets(offsets)
            .setDstSubresource({vk::ImageAspectFlagBits::eColor, i, 0, 1}).setDstOffsets(dstOffsets);
        commandBuffer.blitImage(image.getHandle(), vk::ImageLayout::eTransferSrcOptimal,
                                image.getHandle(), vk::ImageLayout::eTransferDstOptimal, {blit}, filter_);

        barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead).setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mipWidth  > 1) mipWidth  /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    barrier.subresourceRange.setBaseMipLevel(mipLevels_ - 1);
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

    pool_.endSingleTimeCommands(std::move(commandBuffer));
}

}  // namespace resource
