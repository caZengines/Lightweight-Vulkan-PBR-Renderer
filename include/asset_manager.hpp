#pragma once
#include "generic/texture.hpp"
#include "generic/mesh.hpp"
#include <map>
#include <memory>

class AssetManager {
    public:
    void loadTexture(const std::string& path,
                         VmaAllocator* alloc,
                         vk::Format format,
                         vk::Filter filter,
                         CommandPool& cmdPool);

    void loadMesh(const std::string& path, VmaAllocator* alloc, CommandPool& cmdPool);

    std::shared_ptr<const Texture> getTexture(const std::string& key) const;
    std::shared_ptr<const Mesh>    getMesh(const std::string& key) const;

    std::shared_ptr<const Texture> findTexture(const std::string& key) const;
    std::shared_ptr<const Mesh>    findMesh(const std::string& key)    const;
    
    private:
        std::map<std::string, std::shared_ptr<Texture>>         textureCache_;
        std::map<std::string, std::shared_ptr<Mesh>>            meshCache_;
};