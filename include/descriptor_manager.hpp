#pragma once
#include "render_context.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>


class DescriptorSetLayout {
    public:
        explicit DescriptorSetLayout(RenderContext& rct);
        ~DescriptorSetLayout() = default;

        vk::raii::DescriptorSetLayout& getDescriptorSetLayout() { return descriptorSetLayout; }

    private:
        RenderContext                            rct_;
        vk::raii::DescriptorSetLayout            descriptorSetLayout  = nullptr; 

        void createDescriptorSetLayout();
};

class DescriptorPool {
    public:
        //Ban copying
        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;

        explicit DescriptorPool(RenderContext& rct, vk::raii::DescriptorSetLayout& descriptorSetLayout);
        ~DescriptorPool() = default;

        const vk::raii::DescriptorPool& getDescriptorPool() const { return descriptorPool; }

    private:
        RenderContext                            rct_;
        vk::raii::DescriptorPool                 descriptorPool      = nullptr;
};