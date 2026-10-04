#include "resource/shader_library.hpp"

#include <cassert>
#include <fstream>
#include <map>
#include <stdexcept>

#include "extern/spirv_reflect.h"
#include "render_context.hpp"

namespace resource {
namespace {

std::vector<uint8_t> readSpirv(const std::string& absolutePath) {
    std::ifstream file(absolutePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader file: " + absolutePath);
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<uint8_t> code(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
    return code;
}

const char* stageName(vk::ShaderStageFlagBits stage) {
    switch (stage) {
        case vk::ShaderStageFlagBits::eVertex:   return "vertex";
        case vk::ShaderStageFlagBits::eFragment: return "fragment";
        case vk::ShaderStageFlagBits::eCompute:  return "compute";
        default:                                 return "other";
    }
}

std::string describeEntryPoints(
    const std::vector<std::pair<std::string, vk::ShaderStageFlagBits>>& entryPoints) {
    std::string out;
    for (const auto& [name, stage] : entryPoints) {
        if (!out.empty()) out += ", ";
        out += name;
        out += " (";
        out += stageName(stage);
        out += ")";
    }
    return out.empty() ? std::string("<none>") : out;
}

}  // namespace

ShaderLibrary::ShaderLibrary(RenderContext& rct)
    : rct_(rct) {}

const ShaderLibrary::Module& ShaderLibrary::entry(std::string_view spirvPath) const {
    const std::string key{spirvPath};
    if (const auto it = entries_.find(key); it != entries_.end()) {
        return it->second;
    }

    const std::vector<uint8_t> code = readSpirv(key);

    const spv_reflect::ShaderModule reflect(code);
    if (reflect.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error("SPIRV-Reflect failed to parse: " + key);
    }

    std::vector<std::pair<std::string, vk::ShaderStageFlagBits>> entryPoints;
    std::vector<ReflectBinding>                                  bindings;
    std::optional<ReflectPushConstant>                           pushConstant;

    std::map<std::pair<uint32_t, uint32_t>, vk::ShaderStageFlags> stageMap;

    for (uint32_t ep = 0; ep < reflect.GetEntryPointCount(); ++ep) {
        const char* epName = reflect.GetEntryPointName(ep);
        if (epName == nullptr) {
            throw std::runtime_error("SPIRV-Reflect returned a null entry point name in " + key);
        }
        const auto epStageBits = reflect.GetEntryPointShaderStage(ep);
        const auto epStage     = static_cast<vk::ShaderStageFlags>(epStageBits);
        entryPoints.emplace_back(epName, static_cast<vk::ShaderStageFlagBits>(epStageBits));

        uint32_t pcCount = 0;
        reflect.EnumerateEntryPointPushConstantBlocks(epName, &pcCount, nullptr);
        if (pcCount > 0) {
            std::vector<SpvReflectBlockVariable*> blocks(pcCount);
            reflect.EnumerateEntryPointPushConstantBlocks(epName, &pcCount, blocks.data());
            for (const auto* blk : blocks) {
                if (!pushConstant) {
                    pushConstant = ReflectPushConstant{.size       = blk->size,
                                                       .offset     = blk->offset,
                                                       .stageFlags = epStage};
                } 
                else {
                    assert(pushConstant->size == blk->size);
                    assert(pushConstant->offset == blk->offset);
                    pushConstant->stageFlags |= epStage;
                }
            }
        }

        uint32_t bindCount = 0;
        reflect.EnumerateEntryPointDescriptorBindings(epName, &bindCount, nullptr);
        if (bindCount > 0) {
            std::vector<SpvReflectDescriptorBinding*> epBindings(bindCount);
            reflect.EnumerateEntryPointDescriptorBindings(epName, &bindCount, epBindings.data());
            for (const auto* b : epBindings) {
                stageMap[{b->set, b->binding}] |= epStage;
            }
        }
    }

    uint32_t setCount = 0;
    reflect.EnumerateDescriptorSets(&setCount, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(setCount);
    reflect.EnumerateDescriptorSets(&setCount, sets.data());

    for (const auto* set : sets) {
        for (uint32_t bi = 0; bi < set->binding_count; ++bi) {
            const auto* b = set->bindings[bi];
            bindings.emplace_back(
                ReflectBinding()
                    .setBinding(b->binding)
                    .setDescriptorSet(b->set)
                    .setDescriptorType(static_cast<vk::DescriptorType>(b->descriptor_type))
                    .setDescriptorCount(b->count)
                    .setShaderStage(stageMap[{b->set, b->binding}])
                    .setVarName(b->name ? b->name : "")
                    .setBlockSize(b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                      ? b->block.size
                                      : 0)
            );
        }
    }

    vk::ShaderModuleCreateInfo ci{};
    ci.setCodeSize(code.size()).setPCode(reinterpret_cast<const uint32_t*>(code.data()));

    Module newModule{
        .module       = vk::raii::ShaderModule(rct_.device, ci),
        .entryPoints  = std::move(entryPoints),
        .bindings     = std::move(bindings),
        .pushConstant = std::move(pushConstant),
    };
    return entries_.emplace(key, std::move(newModule)).first->second;
}

vk::PipelineShaderStageCreateInfo ShaderLibrary::stage(
    std::string_view spirvPath, std::string_view entryName) const {
    const Module& module = entry(spirvPath);

    for (const auto& [name, stageBits] : module.entryPoints) {
        if (name == entryName) {
            vk::PipelineShaderStageCreateInfo info{};
            info.setStage(stageBits)
                .setModule(*module.module)
                .setPName(name.c_str());
            return info;
        }
    }

    throw std::runtime_error("entry point '" + std::string(entryName) + "' not found in " +
                             std::string(spirvPath) +
                             "; available: " + describeEntryPoints(module.entryPoints));
}

std::span<const ReflectBinding> ShaderLibrary::bindings(std::string_view spirvPath) const {
    return entry(spirvPath).bindings;
}

const std::optional<ReflectPushConstant>& ShaderLibrary::pushConstant(
    std::string_view spirvPath) const {
    return entry(spirvPath).pushConstant;
}

}  // namespace resource
