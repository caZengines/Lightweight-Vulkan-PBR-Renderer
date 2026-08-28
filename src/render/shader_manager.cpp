#include "render/shader_manager.hpp"

#include <fstream>
#include <stdexcept>

namespace render {

const std::vector<uint8_t>& ShaderManager::spirv(std::string_view absolutePath) const {
    const std::string key{absolutePath};
    if (auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }

    std::ifstream file(key, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + key);
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<uint8_t> code(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
    return cache_.emplace(std::move(key), std::move(code)).first->second;
}

vk::raii::ShaderModule ShaderManager::createModule(
    const vk::raii::Device& device,
    const std::vector<uint8_t>& code) const {
    vk::ShaderModuleCreateInfo ci{};
    ci.setCodeSize(code.size())
      .setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    return vk::raii::ShaderModule(device, ci);
}

}  // namespace render
