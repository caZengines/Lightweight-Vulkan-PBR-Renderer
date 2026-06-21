#pragma once
#include "vulkan/vulkan.hpp"
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp> 

class ResourceFactory{
    public:
        //Ban copying and movement
        ResourceFactory(const ResourceFactory&) = delete;
        ResourceFactory& operator=(const ResourceFactory&) = delete;

        static ResourceFactory& get() {
            static ResourceFactory instance_;
            return instance_;
        }
        static void init(vk::raii::PhysicalDevice& pd, vk::raii::Device& device_){
            auto& inst = get();
            inst.physicalDevice = &pd;
            inst.device         = &device_;
        }

        

        [[nodiscard("Factory waring: Ignoring return value of nodiscard function: Image")]]
        std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, uint32_t _mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::SharingMode mode = vk::SharingMode::eExclusive, const std::vector<uint32_t>& queueFamilies = {}) const;

        [[nodiscard("Factory waring: Ignoring return value of nodiscard function: ImageView")]]
        vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels_) const;

        [[nodiscard("Factory: Ignoring return value of nodiscard function: Buffer")]]
        std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::SharingMode mode = vk::SharingMode::eExclusive, const std::vector<uint32_t>& queueFamilyIndices = {}) const;

        vk::raii::Sampler createSampler(vk::SamplerCreateInfo samplerInfo_) const;

        void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height) const;

        void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels_) const;

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

        vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

        vk::FormatProperties getFormatProperties(vk::Format format) {assert(physicalDevice); return physicalDevice->getFormatProperties(format); }
    
        private:
            ResourceFactory() = default;
            vk::raii::PhysicalDevice*                physicalDevice = nullptr;
            vk::raii::Device*                        device = nullptr;
};