#include "asset_manager.hpp"
#include "generic/texture.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

void AssetManager::loadTexture(const std::string& path,
                               vk::Format format, 
                               vk::Filter filter,
                               CommandPool& cmdPool)
{
    std::shared_ptr<Texture> texture_ = 
            std::make_shared<Texture>(Texture::createTexture(path, format, filter, cmdPool));
    textureCache_.insert({path, std::move(texture_)});
}

void AssetManager::loadMesh(const std::string& path, CommandPool& cmdPool){
    std::shared_ptr<Mesh> mesh_ = std::make_shared<Mesh>(path, cmdPool);
    std::cout <<"Number of " <<path <<" vertices: " <<mesh_->getVertices().size() <<"\n";
    meshCache_.insert({path, std::move(mesh_)});
}

std::shared_ptr<const Texture> AssetManager::getTexture(const std::string& key) const {
    auto it = textureCache_.find(key);
    if(it != textureCache_.end()){
        return it->second;
    }
    else {
        throw std::runtime_error("AssetManager: Failed to find texture: " + key);
    }
}

std::shared_ptr<const Texture> AssetManager::findTexture(const std::string& key) const {
    auto it = textureCache_.find(key);
    return (it != textureCache_.end()) ? it->second : nullptr;
}

std::shared_ptr<const Mesh> AssetManager::getMesh(const std::string& key) const {
    auto it = meshCache_.find(key);
    if(it != meshCache_.end()){
        return it->second;
    }
    else {
        throw std::runtime_error("AssetManager: Failed to find mesh: " + key);
    }
}

std::shared_ptr<const Mesh> AssetManager::findMesh(const std::string& key) const {
    auto it = meshCache_.find(key);
    return (it != meshCache_.end()) ? it->second : nullptr;
}