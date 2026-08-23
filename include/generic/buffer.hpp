#pragma once
#include "command_manager.hpp"
#include "resource/upload_queue.hpp"
#include "vma_allocator.hpp"

#include <vector>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

template<typename T> class Buffer{
    public:
        struct CreateInfo{
            vk::DeviceSize size;
            vk::BufferUsageFlags usage;
        };

        // Ban copying; allow moving
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) noexcept = default;
        Buffer& operator=(Buffer&&) noexcept = default;

        // Create a device-local buffer and upload `data` through the
        // UploadQueue (staging + single-time submit live in resource layer —
        // Phase 2: buffers no longer create/upload themselves).
        explicit Buffer(VmaAllocator alloc, const std::vector<T>& data_,
                        const CreateInfo& info, resource::UploadQueue& queue)
            : vmaBuffer_(queue.uploadBuffer(alloc, data_.data(), info.size, info.usage)) {}

        ~Buffer() = default;

        const VkBuffer getBuffer() const { return vmaBuffer_.getHandle(); }

    private:
        VmaBuffer vmaBuffer_;
};
