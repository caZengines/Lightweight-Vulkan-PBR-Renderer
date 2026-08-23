#include "resource/asset_library.hpp"

#include "resource/mesh_importer.hpp"
#include "resource/texture_importer.hpp"
#include "platform/log.hpp"

#include <stdexcept>
#include <utility>

namespace resource {

AssetHandle AssetLibrary::loadMesh(const std::string& path) {
    auto it = paths_.find(path);
    if (it != paths_.end()) {
        ++refCounts_[it->second];
        platform::LogLocator::get().write(platform::LogLevel::Info,
            "AssetLibrary: mesh cache hit (no re-upload): " + path);
        return AssetHandle(this, it->second);
    }

    MeshData data = MeshImporter::load(path);
    const uint32_t id = registry_.createMeshGPU(data);
    paths_[path] = id;
    pathById_[id] = path;
    refCounts_[id] = 1;
    platform::LogLocator::get().write(platform::LogLevel::Info,
        "AssetLibrary: imported mesh: " + path + " (" +
        std::to_string(data.vertices().size()) + " vertices, " +
        std::to_string(data.indices().size()) + " indices)");
    return AssetHandle(this, id);
}

AssetHandle AssetLibrary::loadImage(const std::string& path, vk::Format format, vk::Filter filter) {
    auto it = paths_.find(path);
    if (it != paths_.end()) {
        ++refCounts_[it->second];
        platform::LogLocator::get().write(platform::LogLevel::Info,
            "AssetLibrary: image cache hit (no re-upload): " + path);
        return AssetHandle(this, it->second);
    }

    ImageData image = TextureImporter::load(path);
    const uint32_t id = registry_.createTextureGPU(image, format, filter);
    paths_[path] = id;
    pathById_[id] = path;
    refCounts_[id] = 1;
    platform::LogLocator::get().write(platform::LogLevel::Info,
        "AssetLibrary: imported image: " + path + " (" +
        std::to_string(image.width()) + "x" + std::to_string(image.height()) + ")");
    return AssetHandle(this, id);
}

AssetHandle AssetLibrary::findMesh(const std::string& path) {
    auto it = paths_.find(path);
    if (it == paths_.end()) {
        return AssetHandle();  // Null Object — empty handle
    }
    ++refCounts_[it->second];
    return AssetHandle(this, it->second);
}

AssetHandle AssetLibrary::findImage(const std::string& path) {
    auto it = paths_.find(path);
    if (it == paths_.end()) {
        return AssetHandle();  // Null Object — empty handle
    }
    ++refCounts_[it->second];
    return AssetHandle(this, it->second);
}

void AssetLibrary::retain(uint32_t id) {
    ++refCounts_[id];
}

void AssetLibrary::release(uint32_t id) {
    auto it = refCounts_.find(id);
    if (it == refCounts_.end()) {
        // Double release / release after an explicit unload — ignore.
        return;
    }
    if (--it->second == 0) {
        refCounts_.erase(it);
        registry_.unregister(id);
        auto p = pathById_.find(id);
        if (p != pathById_.end()) {
            paths_.erase(p->second);
            pathById_.erase(p);
            platform::LogLocator::get().write(platform::LogLevel::Info,
                "AssetLibrary: unloaded asset id=" + std::to_string(id));
        }
    }
}

}  // namespace resource
