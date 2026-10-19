#pragma once 
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>


namespace rhi{

class CommandPool {
    public:
        // Ban copying
        CommandPool (const CommandPool&) = delete;
        CommandPool& operator=(const CommandPool&) = delete;

        explicit CommandPool(vk::raii::Device& device, const uint32_t& queueIndex, vk::raii::Queue&& queue, vk::CommandPoolCreateFlags createFlag);
        ~CommandPool() = default;

        vk::raii::CommandPool& setCommandPool() { return commandPool; }

        vk::raii::Queue& queue() { return queue_; }

        vk::raii::CommandBuffer beginSingleTimeCommands();

        void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer);

    private:
        vk::raii::Device*                        device_     = nullptr;
        vk::raii::CommandPool                    commandPool = nullptr;
        vk::raii::Queue                          queue_      = nullptr;
};

}