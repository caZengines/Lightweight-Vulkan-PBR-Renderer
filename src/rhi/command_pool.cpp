#include "rhi/command_pool.hpp"

namespace rhi {

CommandPool::CommandPool(vk::raii::Device& device, const uint32_t& queueIndex, vk::raii::Queue&& queue, vk::CommandPoolCreateFlags createFlags)
    : device_(&device), queue_(std::move(queue))
{

    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.setFlags(createFlags)
            .setQueueFamilyIndex(queueIndex);
    commandPool = vk::raii::CommandPool(device, poolInfo);
}

vk::raii::CommandBuffer CommandPool::beginSingleTimeCommands() {
    assert(device_);
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
    vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(*device_, allocInfo).front());
    //record command
    vk::CommandBufferBeginInfo beginInfo; beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}
void CommandPool::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer) {
    assert(device_);
    commandBuffer.end();
    //wait for submit
    vk::FenceCreateInfo fenceInfo{};
    vk::raii::Fence commandFence(*device_, fenceInfo);

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(*commandBuffer);
    queue_.submit(submitInfo, commandFence);
    (void)device_->waitForFences({commandFence}, VK_TRUE, UINT64_MAX);
}

}
