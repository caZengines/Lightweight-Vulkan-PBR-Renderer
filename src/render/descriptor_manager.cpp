#include "render/descriptor_manager.hpp"

#include <map>
#include <utility>

#include "extern/spirv_reflect.h"

namespace render {DescriptorSetLayout::DescriptorSetLayout(RenderContext& rct, const std::vector<std::uint8_t>& spvCode)
    : rct_(rct) {
    autoCreateDSL(spvCode);
}

void DescriptorSetLayout::autoCreateDSL(const std::vector<std::uint8_t>& spvCode) {
    spv_reflect::ShaderModule module(spvCode);

    std::uint32_t setCountRaw = 0;
    module.EnumerateDescriptorSets(&setCountRaw, nullptr);
    setCount_ = static_cast<int>(setCountRaw);
    std::vector<SpvReflectDescriptorSet*> sets(setCountRaw);
    module.EnumerateDescriptorSets(&setCountRaw, sets.data());

    std::map<std::pair<std::uint32_t, std::uint32_t>, vk::ShaderStageFlags> stageMap;
    for (std::uint32_t ep = 0; ep < module.GetEntryPointCount(); ++ep) {
        const auto epName  = module.GetEntryPointName(ep);
        const auto epStage = module.GetEntryPointShaderStage(ep);

        std::uint32_t bindCount = 0;
        module.EnumerateEntryPointDescriptorBindings(epName, &bindCount, nullptr);
        std::vector<SpvReflectDescriptorBinding*> epBindings(bindCount);
        module.EnumerateEntryPointDescriptorBindings(epName, &bindCount, epBindings.data());

        for (const auto* b : epBindings) {
            stageMap[std::make_pair(b->set, b->binding)] |=
                static_cast<vk::ShaderStageFlags>(epStage);
        }
    }

    // set index → type → unmultiplied descriptor count
    std::map<std::uint32_t, std::map<vk::DescriptorType, std::uint32_t>> perSetDescCounts;

    for (const auto* set : sets) {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};

        for (std::uint32_t bi = 0; bi < set->binding_count; ++bi) {
            const auto* b   = set->bindings[bi];
            const auto  key = std::make_pair(b->set, b->binding);

            layoutBindings.emplace_back(
                vk::DescriptorSetLayoutBinding()
                    .setBinding(b->binding)
                    .setDescriptorType(static_cast<vk::DescriptorType>(b->descriptor_type))
                    .setDescriptorCount(b->count)
                    .setStageFlags(stageMap[key]));
            bindings_.emplace_back(
                ReflectBinding()
                    .setBinding(b->binding)
                    .setDescriptorSet(b->set)
                    .setDescriptorType(static_cast<vk::DescriptorType>(b->descriptor_type))
                    .setDescriptorCount(b->count)
                    .setShaderStage(stageMap[key])
                    .setEntryPoint(b->name ? b->name : "")
                    .setBlockSize(b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                      ? b->block.size
                                      : 0));

            perSetDescCounts[b->set][static_cast<vk::DescriptorType>(b->descriptor_type)] +=
                b->count;
        }
        layoutInfo.setBindings(layoutBindings);

        descriptorSetLayouts_.emplace_back(
            vk::raii::DescriptorSetLayout(rct_.device, layoutInfo));
    }
    layoutHandles_.reserve(descriptorSetLayouts_.size());
    for (const auto& dsl : descriptorSetLayouts_) {
        layoutHandles_.emplace_back(*dsl);
    }

    // Pool sizing: Set 0 is per-frame (× frames in flight), Set 1+ is
    // per-object (× upper bound for simultaneous materials).
    std::map<vk::DescriptorType, std::uint32_t> totalDescriptors;
    poolMaxSets_ = 0;

    for (const auto& [setIdx, descCounts] : perSetDescCounts) {
        const std::uint32_t multiplier =
            (setIdx == 0) ? render::kMaxFramesInFlight
                          : DescriptorSetLayout::kDefaultObjectMultiplier;
        poolMaxSets_ += static_cast<int>(multiplier);
        for (const auto& [type, count] : descCounts) {
            totalDescriptors[type] += count * multiplier;
        }
    }

    poolSizes_.clear();
    poolSizes_.reserve(totalDescriptors.size());
    for (const auto& [type, count] : totalDescriptors) {
        poolSizes_.emplace_back(vk::DescriptorPoolSize().setType(type).setDescriptorCount(count));
    }
}

int DescriptorSetLayout::computePoolMaxSets(std::uint32_t objectCount) const {
    int total = static_cast<int>(render::kMaxFramesInFlight);  // Set 0
    for (int i = 1; i < setCount_; ++i) {
        total += static_cast<int>(objectCount);
    }
    return total;
}

DescriptorPool::DescriptorPool(RenderContext& rct,
                               int maxSets,
                               const std::vector<vk::DescriptorPoolSize>& poolSizes)
    : rct_(rct) {
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setMaxSets(maxSets)
            .setPoolSizes(poolSizes);

    descriptorPool_ = vk::raii::DescriptorPool(rct_.device, poolInfo);
}

}  // namespace render
