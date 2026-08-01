#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "vma_allocator.hpp"

class ResourceFactory{
    public:
        //Ban copying and movement
        ResourceFactory(const ResourceFactory&) = delete;
        ResourceFactory& operator=(const ResourceFactory&) = delete;

        static ResourceFactory& get() {
            static ResourceFactory factory_;
            return factory_;
        }
        static void init(vk::raii::PhysicalDevice& pd, vk::raii::Device& device){
            auto& inst = get();
            if(inst.initialized_){
                return;
            }
            inst.physicalDevice_ = &pd;
            inst.device_         = &device;
            inst.initialized_ = true;
        }


        [[nodiscard("Factory waring: Ignoring return value of nodiscard function: ImageView")]]
        vk::raii::ImageView createImageView(const VmaImage& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels_) const;

        void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const VmaBuffer &buffer, const VmaImage &image, uint32_t width, uint32_t height) const;

        void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const VmaImage &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels_) const;

        vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

        vk::FormatProperties getFormatProperties(vk::Format format) {assert(physicalDevice_); return physicalDevice_->getFormatProperties(format); }
    
        private:
            ResourceFactory() = default;
            vk::raii::PhysicalDevice*                  physicalDevice_ = nullptr;
            vk::raii::Device*                          device_ = nullptr;

            bool                                initialized_ = false;
};