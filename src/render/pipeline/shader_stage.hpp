#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render::detail {

inline const char* stageLabel(vk::ShaderStageFlagBits stage) {
    switch (stage) {
        case vk::ShaderStageFlagBits::eVertex:   return "vertex";
        case vk::ShaderStageFlagBits::eFragment: return "fragment";
        case vk::ShaderStageFlagBits::eCompute:  return "compute";
        default:                                 return "other";
    }
}

// Guards against a spec whose entry point name resolves to the wrong stage in the
// module (e.g. a compute entry handed to a raster spec).
inline void requireStage(const vk::PipelineShaderStageCreateInfo& stageInfo,
                         vk::ShaderStageFlagBits expected,
                         std::string_view entryPoint,
                         std::string_view spirvPath) {
    if (stageInfo.stage == expected) return;
    throw std::runtime_error("entry point '" + std::string(entryPoint) + "' in " +
                             std::string(spirvPath) + " is a " +
                             stageLabel(stageInfo.stage) + " stage, expected " +
                             stageLabel(expected));
}

}  // namespace render::detail
