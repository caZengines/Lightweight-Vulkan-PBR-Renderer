#pragma once
#include "resource/asset_handle.hpp"
#include "resource/image_data.hpp"
#include "resource/mesh_data.hpp"
#include "resource/upload_queue.hpp"
#include "rhi/vma_allocator.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace rhi {
class RhiFactory;
}  // namespace rhi

namespace resource {

class AssetLibrary;
class ResourceRegistry;

// ---------------------------------------------------------------------------
// MeshGPU — GPU half of a mesh: vertex + index buffers only
// Created and owned by ResourceRegistry; Mesh no longer creates buffers.
// ---------------------------------------------------------------------------
class MeshGPU {
    public:
        MeshGPU(const MeshGPU&) = delete;
        MeshGPU& operator=(const MeshGPU&) = delete;

        VkBuffer vertexBuffer() const { return vertexBuffer_.getHandle(); }
        VkBuffer indexBuffer()  const { return indexBuffer_.getHandle(); }
        uint32_t vertexCount()  const { return vertexCount_; }
        uint32_t indexCount()   const { return indexCount_; }

    private:
        friend class ResourceRegistry;
        MeshGPU() = default;

        rhi::VmaBuffer vertexBuffer_;
        rhi::VmaBuffer indexBuffer_;
        uint32_t       vertexCount_ = 0;
        uint32_t       indexCount_  = 0;
};

// ---------------------------------------------------------------------------
// TextureGPU — GPU half of a texture: image + image view + mip count
// (no CPU pixels). Created and owned by ResourceRegistry; Texture no longer
// creates buffers.
// ---------------------------------------------------------------------------
class TextureGPU {
    public:
        TextureGPU(const TextureGPU&) = delete;
        TextureGPU& operator=(const TextureGPU&) = delete;

        const vk::raii::ImageView& view() const { return textureImageView_; }
        vk::Format format() const { return format_; }
        uint32_t   mipLevels() const { return mipLevels_; }

    private:
        friend class ResourceRegistry;
        TextureGPU() = default;

        rhi::VmaImage            vmaImage_;
        vk::raii::ImageView      textureImageView_ = nullptr;
        vk::Format               format_           = vk::Format::eUndefined;
        uint32_t                 mipLevels_        = 1;
};

// ---------------------------------------------------------------------------
// ResourceRegistry — owns the GPU assets (id → MeshGPU/TextureGPU) and the
// built-in 1×1 fallback textures (Null Object defaults). Creation (GPU upload)
// goes through UploadQueue; path caching/refcounting lives in AssetLibrary.
// ---------------------------------------------------------------------------
class ResourceRegistry {
    public:
        ResourceRegistry(VmaAllocator allocator, UploadQueue& uploadQueue,
                         const rhi::RhiFactory& rhiFactory);
        ResourceRegistry(const ResourceRegistry&) = delete;
        ResourceRegistry& operator=(const ResourceRegistry&) = delete;

        // --- GPU resource factories (upload through UploadQueue) ---
        // Returns the registry id; wrap into an AssetHandle via AssetLibrary.
        uint32_t createMeshGPU(const MeshData& data);
        uint32_t createTextureGPU(const ImageData& image, vk::Format format, vk::Filter filter);

        // --- Lookups (handle must be valid) ---
        const MeshGPU&    mesh(const AssetHandle& handle) const;
        const TextureGPU& texture(const AssetHandle& handle) const;

        // --- Built-in 1×1 fallback textures (Null Object defaults) ---
        // Lazily created on first use; registry-owned, never unloaded.
        const TextureGPU& defaultAlbedo() const;
        const TextureGPU& defaultNormal() const;

        // Remove the GPU asset for `id` (called by AssetLibrary on last release).
        void unregister(uint32_t id);

        VmaAllocator allocator() const { return allocator_; }
        UploadQueue& uploadQueue() { return uploadQueue_; }

    private:
        std::unique_ptr<TextureGPU> buildTextureGPU(const ImageData& image,
                                                    vk::Format format, vk::Filter filter) const;

        VmaAllocator allocator_;
        UploadQueue& uploadQueue_;
        const rhi::RhiFactory& rhiFactory_;

        std::unordered_map<uint32_t, std::unique_ptr<MeshGPU>>    meshes_;
        std::unordered_map<uint32_t, std::unique_ptr<TextureGPU>> textures_;
        uint32_t nextId_ = 1;  // 0 is reserved for the null handle

        mutable std::unique_ptr<TextureGPU> defaultAlbedo_;
        mutable std::unique_ptr<TextureGPU> defaultNormal_;
};

}  // namespace resource
