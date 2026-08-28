#pragma once
#include "rhi/command_pool.hpp"
#include "rhi/vma_allocator.hpp"

#include <cstdint>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace rhi {
class RhiFactory;
}  // namespace rhi

namespace resource {

// One-shot GPU uploads through the transient command pool (Layer 2).
// Owns the staging-buffer + single-submit logic, so MeshGPU/TextureGPU (and
// Buffer<T>) never create or upload buffers themselves.
// Phase 3: view creation / layout transitions come from an injected
// rhi::RhiFactory instead of the removed singleton.
class UploadQueue {
    public:
        explicit UploadQueue(rhi::CommandPool& transientPool, const rhi::RhiFactory& factory,
                             VmaAllocator allocator)
            : pool_(transientPool), factory_(factory), allocator_(allocator) {}

        UploadQueue(const UploadQueue&) = delete;
        UploadQueue& operator=(const UploadQueue&) = delete;

        // Create a device-local buffer of `size` bytes with `usage` and upload
        // `data` through a host-visible staging buffer (one single-time submit).
        rhi::VmaBuffer uploadBuffer(const void* data, vk::DeviceSize size,
                                    vk::BufferUsageFlags usage);

        // Upload `data` into an existing image (created with eTransferDst|eSampled):
        // Undefined → TransferDst → copy → (mipmap generation if mipLevels > 1)
        // → ShaderReadOnly. One single-time submit per stage.
        void uploadImage(const void* data, vk::DeviceSize size,
                         rhi::VmaImage& image, uint32_t width, uint32_t height,
                         uint32_t mipLevels, vk::Format format, vk::Filter filter);

        [[nodiscard]] VmaAllocator allocator() const noexcept { return allocator_; }

        rhi::CommandPool& pool() { return pool_; }

    private:
        rhi::CommandPool& pool_;
        const rhi::RhiFactory& factory_;
        VmaAllocator allocator_;

        // Blit-based mip chain generation.
        void generateMipmaps(rhi::VmaImage& image, vk::Format format, vk::Filter filter,
                             uint32_t width, uint32_t height, uint32_t mipLevels);
};

}  // namespace resource
