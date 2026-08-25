#pragma once
#include "command_manager.hpp"
#include "vma_allocator.hpp"

#include <cstdint>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace resource {

// One-shot GPU uploads through the transient command pool (Layer 2).
// Owns the staging-buffer + single-submit logic, so MeshGPU/TextureGPU (and
// Buffer<T>) never create or upload buffers themselves.
class UploadQueue {
    public:
        explicit UploadQueue(CommandPool& transientPool) : pool_(transientPool) {}

        UploadQueue(const UploadQueue&) = delete;
        UploadQueue& operator=(const UploadQueue&) = delete;

        // Create a device-local buffer of `size` bytes with `usage` and upload
        // `data` through a host-visible staging buffer (one single-time submit).
        VmaBuffer uploadBuffer(VmaAllocator alloc, const void* data,
                               vk::DeviceSize size, vk::BufferUsageFlags usage);

        // Upload `data` into an existing image (created with eTransferDst|eSampled):
        // Undefined → TransferDst → copy → (mipmap generation if mipLevels > 1)
        // → ShaderReadOnly. One single-time submit per stage.
        void uploadImage(VmaAllocator alloc, const void* data, vk::DeviceSize size,
                         VmaImage& image, uint32_t width, uint32_t height,
                         uint32_t mipLevels, vk::Format format, vk::Filter filter);

        CommandPool& pool() { return pool_; }

    private:
        CommandPool& pool_;

        // Blit-based mip chain generation.
        void generateMipmaps(VmaImage& image, vk::Format format, vk::Filter filter,
                             uint32_t width, uint32_t height, uint32_t mipLevels);
};

}  // namespace resource
