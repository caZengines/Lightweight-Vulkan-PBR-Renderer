#pragma once
#include "command_manager.hpp"
#include "vma_allocator.hpp"
#include "vulkan/vulkan.hpp"
#include <vector>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan.hpp>

template<typename T> class Buffer{
    public:
        struct CreateInfo{
            vk::DeviceSize size;
            vk::BufferUsageFlags usage;
            vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
            std::vector<uint32_t> queueFamilyIndices;
        };

        //Ban copying
        Buffer<T>& operator=(const Buffer<T>&) = delete;

        explicit Buffer(VmaAllocator* alloc, const std::vector<T>& data_,const CreateInfo& info, CommandPool& commandPool){
            vk::BufferCreateInfo stagingInfo{};
            stagingInfo.setSize(info.size).setUsage(vk::BufferUsageFlagBits::eTransferSrc);
            VmaAllocationCreateInfo stagingCI{};
            stagingCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            stagingCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            VmaBuffer stagingBuffer(alloc, static_cast<const VkBufferCreateInfo&>(stagingInfo), stagingCI);

            void* dataStaging = stagingBuffer.map();
            memcpy(dataStaging, data_.data(), info.size);
            stagingBuffer.unmap();

            vk::BufferCreateInfo bufferInfo;
            bufferInfo.setSize(info.size).setUsage(info.usage).setSharingMode(info.sharingMode);
            if(info.sharingMode == vk::SharingMode::eConcurrent) {
                bufferInfo.setQueueFamilyIndices(info.queueFamilyIndices);
            }
            VmaAllocationCreateInfo allocCI{};
            allocCI.usage = VMA_MEMORY_USAGE_AUTO;
            vmaBuffer_ = VmaBuffer(alloc, static_cast<const VkBufferCreateInfo&>(bufferInfo), allocCI);
            vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();
            commandBuffer.copyBuffer(stagingBuffer.getHandle(), vmaBuffer_.getHandle(), vk::BufferCopy(0, 0, info.size));
            commandPool.endSingleTimeCommands(std::move(commandBuffer));
        }
        ~Buffer() = default;

        const VkBuffer& getBuffer() const { return vmaBuffer_.getHandle(); }

    private:
        VmaBuffer                                vmaBuffer_;
};