#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// Single home for SPIR-V loading (previously scattered across
// Pipeline::readFile and ad-hoc reads). Decoded blobs are cached per absolute
// path so reflection input (DescriptorSetLayout) and pipeline creation share
// exactly one disk hit per shader.
class ShaderManager {
public:
    ShaderManager() = default;

    // `absolutePath` comes pre-resolved from app::Config.
    [[nodiscard]] const std::vector<std::uint8_t>& spirv(std::string_view absolutePath) const;

    [[nodiscard]] vk::raii::ShaderModule createModule(
        const vk::raii::Device& device,
        const std::vector<std::uint8_t>& code) const;

private:
    // mutable: lazy disk cache behind a logically const lookup.
    mutable std::unordered_map<std::string, std::vector<std::uint8_t>> cache_;
};

}  // namespace render
