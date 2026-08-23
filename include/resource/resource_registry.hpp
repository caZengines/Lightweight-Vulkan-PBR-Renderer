#pragma once
#include "resource/image_data.hpp"
#include "resource/mesh_data.hpp"
#include "resource/upload_queue.hpp"
#include "vma_allocator.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace resource {

class AssetLibrary;
class ResourceRegistry;

// ---------------------------------------------------------------------------
// AssetHandle — opaque, refcounted reference to a registry-owned GPU asset.
// A null handle (id == 0) is the Null Object: render code falls back to the
// registry's built-in default assets (GPP Service Locator's Null service).
// Copying a handle retains the asset; the last release unloads it from the
// registry (refcounting lives in AssetLibrary).
// ---------------------------------------------------------------------------
class AssetHandle {
    public:
        AssetHandle() noexcept = default;
        AssetHandle(const AssetHandle& other);
        AssetHandle& operator=(const AssetHandle& other);
        AssetHandle(AssetHandle&& other) noexcept;
        AssetHandle& operator=(AssetHandle&& other) noexcept;
        ~AssetHandle();

        uint32_t id() const noexcept { return id_; }
        bool     valid() const noexcept { return id_ != 0; }
        explicit operator bool() const noexcept { return valid(); }
        bool operator==(const AssetHandle&) const noexcept = default;

    private:
        friend class AssetLibrary;
        AssetHandle(AssetLibrary* library, uint32_t id) noexcept;

        AssetLibrary* library_ = nullptr;
        uint32_t      id_      = 0;
};

// ---------------------------------------------------------------------------
// MeshGPU — GPU half of a mesh: vertex + index buffers only (no CPU data).
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

        VmaBuffer vertexBuffer_;
        VmaBuffer indexBuffer_;
        uint32_t  vertexCount_ = 0;
        uint32_t  indexCount_  = 0;
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

        VmaImage                 vmaImage_;
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
        ResourceRegistry(VmaAllocator allocator, UploadQueue& uploadQueue);
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

        std::unordered_map<uint32_t, std::unique_ptr<MeshGPU>>    meshes_;
        std::unordered_map<uint32_t, std::unique_ptr<TextureGPU>> textures_;
        uint32_t nextId_ = 1;  // 0 is reserved for the null handle

        mutable std::unique_ptr<TextureGPU> defaultAlbedo_;
        mutable std::unique_ptr<TextureGPU> defaultNormal_;
};

}  // namespace resource
