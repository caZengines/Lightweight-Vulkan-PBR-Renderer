#include "generic/texture.hpp" 
#include "resourcefactory.hpp"
#include "vma_allocator.hpp"
#include "vulkan/vulkan.hpp"
#include <cmath>

#include <ktx.h>
#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb_image.h"

// ============================================================================
// Default 1×1 textures (CPU-generated, no file I/O)
// ============================================================================

Texture Texture::createDefaultTexture(VmaAllocator* alloc, const void* pixelDataRGBA8, vk::Format format, CommandPool& commandPool) {
    auto& factory = ResourceFactory::get();
    Texture tex;
    tex.mipLevels = 1;

    constexpr uint32_t      kWidth  = 1;
    constexpr uint32_t      kHeight = 1;
    constexpr vk::DeviceSize kSize  = 4;  // RGBA8 = 4 bytes
        vk::BufferCreateInfo stagingInfo{};
        stagingInfo.setSize(kSize).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
        VmaAllocationCreateInfo stagingCI{};
        stagingCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        stagingCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        VmaBuffer stagingBuffer(alloc, static_cast<const VkBufferCreateInfo&>(stagingInfo), stagingCI);

        void* dataStaging = stagingBuffer.map();
        memcpy(dataStaging, pixelDataRGBA8, kSize);
        stagingBuffer.unmap();

    vk::ImageCreateInfo imageCI{};
    imageCI.setExtent({kWidth, kHeight, 1})
           .setFormat(format)
           .setMipLevels(1).setSamples(vk::SampleCountFlagBits::e1)
           .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
           .setTiling(vk::ImageTiling::eOptimal)
           .setArrayLayers(1)
           .setImageType(vk::ImageType::e2D);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    tex.vmaImage_ = VmaImage(alloc, static_cast<const VkImageCreateInfo&>(imageCI), allocCI);

    vk::raii::CommandBuffer cmd = commandPool.beginSingleTimeCommands();
    factory.transitionImageLayout(cmd, tex.vmaImage_,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal, 1);
    factory.copyBufferToImage(cmd, stagingBuffer, tex.vmaImage_, kWidth, kHeight);
    factory.transitionImageLayout(cmd, tex.vmaImage_,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::ImageLayout::eShaderReadOnlyOptimal, 1);
    commandPool.endSingleTimeCommands(std::move(cmd));

    tex.textureImageView = factory.createImageView(tex.vmaImage_, format,
                                                   vk::ImageAspectFlagBits::eColor, 1);
    return tex;
}

Texture Texture::createDefaultAlbedo(VmaAllocator* alloc, CommandPool& commandPool) {
    // 1×1 opaque white: (R=255, G=255, B=255, A=255)
    constexpr uint8_t kWhite[4] = {255, 255, 255, 255};
    return createDefaultTexture(alloc, kWhite, vk::Format::eR8G8B8A8Srgb, commandPool);
}

Texture Texture::createDefaultNormal(VmaAllocator* alloc, CommandPool& commandPool) {
    // 1×1 flat tangent-space normal pointing straight up: (R=128, G=128, B=255, A=255)
    // Decoded: x = 128/255*2-1 = 0, y = 128/255*2-1 = 0, z = 255/255 = 1  →  (0,0,1)
    constexpr uint8_t kFlatNormal[4] = {128, 128, 255, 255};
    return createDefaultTexture(alloc, kFlatNormal, vk::Format::eR8G8B8A8Unorm, commandPool);
}

// ============================================================================
// File-based texture loading
// ============================================================================

Texture Texture::createTexture(const std::string &filepath, VmaAllocator* alloc, vk::Format textureFormat, vk::Filter filter, CommandPool& commandPool){
    auto& factory = ResourceFactory::get();
    Texture tex;

    int            texWidth, texHeight, texChannel;
    stbi_uc       *pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannel, STBI_rgb_alpha);
    if(!pixels){
        throw std::runtime_error("failed to load texture image! ");
    }
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) +1;
    
        vk::BufferCreateInfo stagingInfo{};
        stagingInfo.setSize(imageSize).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
        VmaAllocationCreateInfo stagingCI{};
        stagingCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        stagingCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        VmaBuffer stagingBuffer(alloc, static_cast<const VkBufferCreateInfo&>(stagingInfo), stagingCI);

        void* dataStaging = stagingBuffer.map();
        memcpy(dataStaging, pixels, imageSize);
        stagingBuffer.unmap();
    stbi_image_free(pixels);

    vk::ImageCreateInfo imageCI{};
    imageCI.setExtent({static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1})
           .setMipLevels(tex.mipLevels).setSamples(vk::SampleCountFlagBits::e1)
           .setFormat(textureFormat).setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
           .setArrayLayers(1)
           .setImageType(vk::ImageType::e2D);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    tex.vmaImage_ = VmaImage(alloc, static_cast<const VkImageCreateInfo&>(imageCI), allocCI);

    vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();
    factory.transitionImageLayout(commandBuffer, tex.vmaImage_, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);
    factory.copyBufferToImage(commandBuffer, stagingBuffer, tex.vmaImage_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    commandPool.endSingleTimeCommands(std::move(commandBuffer));
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
    tex.generateMipMaps(tex.vmaImage_, textureFormat, filter, texWidth, texHeight, tex.mipLevels, factory, commandPool);

    tex.textureImageView = factory.createImageView(tex.vmaImage_, textureFormat, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

    return tex;
}

void Texture::generateMipMaps(const VmaImage& image, vk::Format imageFormat, vk::Filter filter_, uint32_t texWidth_, uint32_t texHeight_, uint32_t mipLevels_, ResourceFactory& factory_, CommandPool& commandPool){
    vk::FormatProperties formatProperties = factory_.getFormatProperties(imageFormat);
    if(!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)){
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier;
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eTransferRead)
           .setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
           .setDstQueueFamilyIndex(vk::QueueFamilyIgnored).setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image.getHandle());
    barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor).setBaseArrayLayer(0).setLayerCount(1).setLevelCount(1);

    uint32_t mipWidth  = texWidth_;
    uint32_t mipHeight = texHeight_;
    for(uint32_t i = 1 ; i < mipLevels_ ;++i){
        barrier.subresourceRange.setBaseMipLevel(i - 1);
        barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eTransferSrcOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eTransferRead);

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

    vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
    offsets[0] = vk::Offset3D(0, 0, 0);
    offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
    dstOffsets[0] = vk::Offset3D(0, 0, 0);
    dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth/2 : 1, mipHeight > 1 ? mipHeight/2 : 1, 1);
    vk::ImageBlit blit; blit.setSrcSubresource({vk::ImageAspectFlagBits::eColor, i-1, 0, 1}).setSrcOffsets(offsets)
                                        .setDstSubresource({vk::ImageAspectFlagBits::eColor, i, 0, 1}).setDstOffsets(dstOffsets);
    commandBuffer.blitImage(image.getHandle(), vk::ImageLayout::eTransferSrcOptimal, image.getHandle(), vk::ImageLayout::eTransferDstOptimal, {blit}, filter_);

    //barrier.subresourceRange.setBaseMipLevel(i - 1);
    barrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead).setDstAccessMask(vk::AccessFlagBits::eShaderRead);
                
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if(mipWidth  > 1) mipWidth  /= 2;
        if(mipHeight > 1) mipHeight /= 2;
    }
    barrier.subresourceRange.setBaseMipLevel(mipLevels_ - 1);
    barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
            
    commandPool.endSingleTimeCommands(std::move(commandBuffer));
}
