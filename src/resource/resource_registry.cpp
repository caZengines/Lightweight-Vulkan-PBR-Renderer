#include "resource/resource_registry.hpp"

#include "resource/asset_library.hpp"
#include "resourcefactory.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace resource {

// ===========================================================================
// AssetHandle — refcounting is delegated to the AssetLibrary that minted it.
// ===========================================================================

AssetHandle::AssetHandle(AssetLibrary* library, uint32_t id) noexcept
    : library_(library), id_(id) {}

AssetHandle::AssetHandle(const AssetHandle& other)
    : library_(other.library_), id_(other.id_) {
    if (library_ && id_) {
        library_->retain(id_);
    }
}

AssetHandle& AssetHandle::operator=(const AssetHandle& other) {
    AssetHandle temp(other);
    this->swap(temp);
    return *this;
}

AssetHandle::AssetHandle(AssetHandle&& other) noexcept
    : library_(std::exchange(other.library_, nullptr)),
      id_(std::exchange(other.id_, 0)) {}

AssetHandle& AssetHandle::operator=(AssetHandle&& other) noexcept {
    this->swap(other);
    return *this;
}

AssetHandle::~AssetHandle() {
    if (library_ && id_) {
        library_->release(id_);
    }
}

// ===========================================================================
// ResourceRegistry
// ===========================================================================

ResourceRegistry::ResourceRegistry(VmaAllocator allocator, UploadQueue& uploadQueue)
    : allocator_(allocator), uploadQueue_(uploadQueue) {}

uint32_t ResourceRegistry::createMeshGPU(const MeshData& data) {
    if (data.empty()) {
        throw std::runtime_error("ResourceRegistry: cannot create MeshGPU from empty MeshData");
    }

    std::unique_ptr<MeshGPU> gpu(new MeshGPU());
    gpu->vertexCount_ = static_cast<uint32_t>(data.vertices().size());
    gpu->indexCount_  = static_cast<uint32_t>(data.indices().size());

    gpu->vertexBuffer_ = uploadQueue_.uploadBuffer(
        allocator_, data.vertices().data(),
        sizeof(Vertex) * data.vertices().size(),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    gpu->indexBuffer_ = uploadQueue_.uploadBuffer(
        allocator_, data.indices().data(),
        sizeof(uint32_t) * data.indices().size(),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    const uint32_t id = nextId_++;
    meshes_[id] = std::move(gpu);
    return id;
}

uint32_t ResourceRegistry::createTextureGPU(const ImageData& image,
                                            vk::Format format, vk::Filter filter) {
    auto gpu = buildTextureGPU(image, format, filter);
    const uint32_t id = nextId_++;
    textures_[id] = std::move(gpu);
    return id;
}

std::unique_ptr<TextureGPU> ResourceRegistry::buildTextureGPU(const ImageData& image,
                                                              vk::Format format,
                                                              vk::Filter filter) const {
    auto& factory = ResourceFactory::get();
    auto  gpu     = std::unique_ptr<TextureGPU>(new TextureGPU());
    gpu->format_ = format;

    const uint32_t width  = image.width();
    const uint32_t height = image.height();
    gpu->mipLevels_ = (width == 0 || height == 0)
                          ? 1u
                          : static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    vk::ImageCreateInfo imageCI{};
    imageCI.setExtent({width, height, 1})
           .setFormat(format)
           .setMipLevels(gpu->mipLevels_)
           .setSamples(vk::SampleCountFlagBits::e1)
           .setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eTransferDst |
                     vk::ImageUsageFlagBits::eSampled)
           .setArrayLayers(1)
           .setImageType(vk::ImageType::e2D);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    gpu->vmaImage_ = VmaImage(allocator_, static_cast<const VkImageCreateInfo&>(imageCI), allocCI);

    uploadQueue_.uploadImage(allocator_, image.pixels().data(),
                             static_cast<vk::DeviceSize>(image.pixels().size()),
                             gpu->vmaImage_, width, height, gpu->mipLevels_, format, filter);

    gpu->textureImageView_ = factory.createImageView(gpu->vmaImage_, format,
                                                     vk::ImageAspectFlagBits::eColor,
                                                     gpu->mipLevels_);
    return gpu;
}

const MeshGPU& ResourceRegistry::mesh(const AssetHandle& handle) const {
    auto it = meshes_.find(handle.id());
    if (it == meshes_.end()) {
        throw std::runtime_error("ResourceRegistry: invalid mesh handle (id=" +
                                 std::to_string(handle.id()) + ")");
    }
    return *it->second;
}

const TextureGPU& ResourceRegistry::texture(const AssetHandle& handle) const {
    auto it = textures_.find(handle.id());
    if (it == textures_.end()) {
        throw std::runtime_error("ResourceRegistry: invalid texture handle (id=" +
                                 std::to_string(handle.id()) + ")");
    }
    return *it->second;
}

const TextureGPU& ResourceRegistry::defaultAlbedo() const {
    if (!defaultAlbedo_) {
        // 1×1 opaque white — same pixels as the old Texture::createDefaultAlbedo.
        constexpr uint8_t kWhite[4] = {255, 255, 255, 255};
        ImageData data(std::vector<uint8_t>(kWhite, kWhite + 4), 1, 1);
        defaultAlbedo_ = buildTextureGPU(data, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);
    }
    return *defaultAlbedo_;
}

const TextureGPU& ResourceRegistry::defaultNormal() const {
    if (!defaultNormal_) {
        // 1×1 flat tangent-space normal (0,0,1): (R=128, G=128, B=255, A=255)
        // — same pixels as the old Texture::createDefaultNormal.
        constexpr uint8_t kFlatNormal[4] = {128, 128, 255, 255};
        ImageData data(std::vector<uint8_t>(kFlatNormal, kFlatNormal + 4), 1, 1);
        defaultNormal_ = buildTextureGPU(data, vk::Format::eR8G8B8A8Unorm, vk::Filter::eNearest);
    }
    return *defaultNormal_;
}

void ResourceRegistry::unregister(uint32_t id) {
    meshes_.erase(id);
    textures_.erase(id);
}

}  // namespace resource
