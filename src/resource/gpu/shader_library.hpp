#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
struct ReflectPushConstant {
    uint32_t             size   = 0;
    uint32_t             offset = 0;
    vk::ShaderStageFlags stageFlags{};
};

struct ReflectBinding {
    uint32_t             binding = 0;
    uint32_t             set = 0;
    vk::DescriptorType   descriptorType{};
    uint32_t             count = 0;
    vk::ShaderStageFlags stageFlags{};
    std::string          name;            // 
    uint32_t             blockSize = 0;   // only vaild for  UBO/SSBO 

    ReflectBinding& setBinding(uint32_t v) { binding = v; return *this; }
    ReflectBinding& setDescriptorSet(uint32_t v) { set = v; return *this; }
    ReflectBinding& setDescriptorType(vk::DescriptorType v) { descriptorType = v; return *this; }
    ReflectBinding& setDescriptorCount(uint32_t v) { count = v; return *this; }
    ReflectBinding& setShaderStage(vk::ShaderStageFlags v) { stageFlags = v; return *this; }
    ReflectBinding& setVarName(std::string v) { name = std::move(v); return *this; }
    ReflectBinding& setBlockSize(uint32_t v) { blockSize = v; return *this; }
};
struct RenderContext;

namespace resource {

class ShaderLibrary final {
public:
    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;
    ShaderLibrary(const ShaderLibrary&&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&&) = delete;

    explicit ShaderLibrary(RenderContext& rct);

    [[nodiscard]] vk::PipelineShaderStageCreateInfo stage(
        std::string_view spirvPath, std::string_view entryName) const;


    [[nodiscard]] std::span<const ReflectBinding> bindings(std::string_view spirvPath) const;

    // The push constant after merging the entry points (size/offset must be consistent, stageFlags take union)
    [[nodiscard]] const std::optional<ReflectPushConstant>& pushConstant(
        std::string_view spirvPath) const;

private:

    struct Module {
        vk::raii::ShaderModule                                       module;
        std::vector<std::pair<std::string, vk::ShaderStageFlagBits>> entryPoints;  // OpEntryPoint name → stage
        std::vector<ReflectBinding>                                  bindings;     // module level，include set 0
        std::optional<ReflectPushConstant>                           pushConstant;
    };

    [[nodiscard]] const Module& entry(std::string_view spirvPath) const;

    RenderContext& rct_;

    mutable std::unordered_map<std::string, Module> entries_;
};

}  // namespace resource
