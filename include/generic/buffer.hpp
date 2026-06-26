#pragma once
#include "command_manager.hpp"
#include "resourcefactory.hpp"
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
            vk::MemoryPropertyFlags memProperties;
            vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
            std::vector<uint32_t> queueFamilyIndices;
        };

        //Ban copying
        Buffer<T>& operator=(const Buffer<T>&) = delete;

        explicit Buffer(const std::vector<T>& data_,const CreateInfo& info, CommandPool& commandPool){
            auto& factory = ResourceFactory::get();

            auto [stagingBuffer, stagingBufferMemory] = 
                        factory.createBuffer(info.size, 
                                    vk::BufferUsageFlagBits::eTransferSrc, 
                                    vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
            );
            void* dataStaging = stagingBufferMemory.mapMemory(0, info.size);
            memcpy(dataStaging, data_.data(), info.size);
            stagingBufferMemory.unmapMemory();

            std::tie(buffer, bufferMemory) = 
                        factory.createBuffer(info.size,
                                     info.usage,
                                     info.memProperties,
                                     info.sharingMode,
                                     info.queueFamilyIndices
            );
            vk::raii::CommandBuffer commandBuffer = commandPool.beginSingleTimeCommands();
            commandBuffer.copyBuffer(*stagingBuffer, *buffer, vk::BufferCopy(0, 0, info.size));
            commandPool.endSingleTimeCommands(std::move(commandBuffer));
        }
        ~Buffer() = default;

        const vk::raii::Buffer& getBuffer() const { return buffer; }

    private:
        vk::raii::DeviceMemory                   bufferMemory = nullptr;
        vk::raii::Buffer                         buffer = nullptr;
};