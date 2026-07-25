#include "generic/texture.hpp" 
#include "resourcefactory.hpp"
#include "vulkan/vulkan.hpp"
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb_image.h"

// ============================================================================
// Default 1×1 textures (CPU-generated, no file I/O)
// ============================================================================

Texture Texture::createDefaultTexture(const void* pixelDataRGBA8, vk::Format format, CommandPool& commandPool) {
    auto& factory = ResourceFactory::get();
    Texture tex;
    tex.mipLevels = 1;

    constexpr uint32_t      kWidth  = 1;
    constexpr uint32_t      kHeight = 1;
    constexpr vk::DeviceSize kSize  = 4;  // RGBA8 = 4 bytes

    auto [stagingBuffer, stagingMemory] =
        factory.createBuffer(kSize,
                             vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent);
    void* dst = stagingMemory.mapMemory(0, kSize);
    std::memcpy(dst, pixelDataRGBA8, kSize);
    stagingMemory.unmapMemory();

    auto [image, imageMemory] =
        factory.createImage(kWidth, kHeight, 1, vk::SampleCountFlagBits::e1,
                            format, vk::ImageTiling::eOptimal,
                            vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eSampled,
                            vk::MemoryPropertyFlagBits::eDeviceLocal);
    tex.textureImage      = std::move(image);
    tex.textureImageMemory = std::move(imageMemory);

    vk::raii::CommandBuffer cmd = commandPool.beginSingleTimeCommands();
    factory.transitionImageLayout(cmd, tex.textureImage,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal, 1);
    factory.copyBufferToImage(cmd, stagingBuffer, tex.textureImage, kWidth, kHeight);
    factory.transitionImageLayout(cmd, tex.textureImage,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::ImageLayout::eShaderReadOnlyOptimal, 1);
    commandPool.endSingleTimeCommands(std::move(cmd));

    tex.textureImageView = factory.createImageView(*tex.textureImage, format,
                                                   vk::ImageAspectFlagBits::eColor, 1);
    return tex;
}

Texture Texture::createDefaultAlbedo(CommandPool& commandPool) {
    // 1×1 opaque white: (R=255, G=255, B=255, A=255)
    constexpr uint8_t kWhite[4] = {255, 255, 255, 255};
    return createDefaultTexture(kWhite, vk::Format::eR8G8B8A8Srgb, commandPool);
}

Texture Texture::createDefaultNormal(CommandPool& commandPool) {
    // 1×1 flat tangent-space normal pointing straight up: (R=128, G=128, B=255, A=255)
    // Decoded: x = 128/255*2-1 = 0, y = 128/255*2-1 = 0, z = 255/255 = 1  →  (0,0,1)
    constexpr uint8_t kFlatNormal[4] = {128, 128, 255, 255};
    return createDefaultTexture(kFlatNormal, vk::Format::eR8G8B8A8Unorm, commandPool);
}

// ============================================================================
// File-based texture loading
// ============================================================================

Texture Texture::createTexture(const std::string &filepath, vk::Format textureFormat, vk::Filter filter, CommandPool& commandPool){
    auto& factory = ResourceFactory::get();
    Texture tex;

    int            texWidth, texHeight, texChannel;
    stbi_uc       *pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannel, STBI_rgb_alpha);
    if(!pixels){
        throw std::runtime_error("failed to load texture image! ");
    }
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) +1;

    auto [stagingBuffer, stagingBufferMemory] = 
            factory.createBuffer(
                imageSize,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                vk::SharingMode::eExclusive
    );
    void *data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();
    stbi_image_free(pixels);

    auto [image, imageMemory] = 
                factory.createImage(
                    texWidth, texHeight, tex.mipLevels, vk::SampleCountFlagBits::e1,
                    textureFormat,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    tex.textureImage = std::move(image);
    tex.textureImageMemory = std::move(imageMemory);

    vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();
    factory.transitionImageLayout(commandBuffer, tex.textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, tex.mipLevels);
    factory.copyBufferToImage(commandBuffer, stagingBuffer, tex.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    commandPool.endSingleTimeCommands(std::move(commandBuffer));
    //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
    tex.generateMipMaps(tex.textureImage, textureFormat, filter, texWidth, texHeight, tex.mipLevels, factory, commandPool);

    tex.textureImageView = factory.createImageView(*tex.textureImage, textureFormat, vk::ImageAspectFlagBits::eColor, tex.mipLevels);

    return tex;
}

void Texture::generateMipMaps(vk::raii::Image& image, vk::Format imageFormat, vk::Filter filter_, uint32_t texWidth_, uint32_t texHeight_, uint32_t mipLevels_, ResourceFactory& factory_, CommandPool& commandPool){
    vk::FormatProperties formatProperties = factory_.getFormatProperties(imageFormat);
    if(!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)){
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier;
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eTransferRead)
           .setOldLayout(vk::ImageLayout::eTransferDstOptimal).setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
           .setDstQueueFamilyIndex(vk::QueueFamilyIgnored).setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
           .setImage(image);
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
    commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, {blit}, filter_);

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
