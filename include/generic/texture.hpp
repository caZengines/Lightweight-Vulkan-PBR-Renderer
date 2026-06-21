#pragma once
#include <string>
#include "resourcefactory.hpp"
#include "command_manager.hpp"
#include "vulkan/vulkan.hpp"

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

class Texture{
    public:
        //Ban copying, allowed movement
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&& other) noexcept = default;
        Texture& operator=(Texture&& other) noexcept = default;

        static Texture createTexture(const std::string& filepath, vk::Format textureFormat, vk::Filter filter, const vk::SamplerCreateInfo& samplerInfo, CommandPool& commandPool);

        const vk::raii::ImageView& getTextureView() const { return textureImageView; }
        const vk::raii::Sampler&   getTextureSampler() const { return textureSampler; }

    private:
        Texture() = default;

        vk::raii::Image                          textureImage         = nullptr;
        vk::raii::DeviceMemory                   textureImageMemory   = nullptr;
        vk::raii::ImageView                      textureImageView     = nullptr;
        vk::raii::Sampler                        textureSampler       = nullptr;

        uint32_t                                 mipLevels            = 1;

        void generateMipMaps(vk::raii::Image& image, vk::Format imageFormat, vk::Filter filter_, uint32_t texWidth_, uint32_t texHeight_, uint32_t mipLevels_, ResourceFactory& , CommandPool& commandPool);
};