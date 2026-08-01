#pragma once
#include <string>
#include "resourcefactory.hpp"
#include "vma_allocator.hpp"
#include "command_manager.hpp"

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>


//It should be noticed that the Texture class needs Manually declare creation
class Texture{
    public:
        //Ban copying, allowed movement
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&& other) noexcept = default;
        Texture& operator=(Texture&& other) noexcept = default;

        static Texture createTexture(const std::string& filepath, VmaAllocator* alloc, vk::Format textureFormat, vk::Filter filter, CommandPool& commandPool);

        // Create a 1×1 white texture – fallback when no albedo is provided.
        static Texture createDefaultAlbedo(VmaAllocator* alloc, CommandPool& commandPool);
        // Create a 1×1 flat-normal texture (0,0,1 in tangent space) – fallback when no normal map is provided.
        static Texture createDefaultNormal(VmaAllocator* alloc, CommandPool& commandPool);

        const vk::raii::ImageView& getTextureView() const { return textureImageView; }

    private:
        Texture() = default;

        // Shared implementation for 1×1 default textures.
        static Texture createDefaultTexture(VmaAllocator* alloc, const void* pixelDataRGBA8, vk::Format format, CommandPool& commandPool);

        VmaImage                                 vmaImage_;
        vk::raii::ImageView                      textureImageView     = nullptr;

        uint32_t                                 mipLevels            = 1;

        void generateMipMaps(const VmaImage& image, vk::Format imageFormat, vk::Filter filter_, uint32_t texWidth_, uint32_t texHeight_, uint32_t mipLevels_, ResourceFactory& , CommandPool& commandPool);
};