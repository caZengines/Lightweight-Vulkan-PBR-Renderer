#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

class Sampler {
    public:
        Sampler(const vk::raii::Device& device, const vk::SamplerCreateInfo& samplerInfo);

        // This interface returns Sampler handle
        const vk::Sampler getSampler() const { return *sampler; }
    private:
        vk::raii::Sampler           sampler = nullptr;
};