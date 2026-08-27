#include "descriptor_manager.hpp"
#include "render_context.hpp"
#include <map>
#include <utility>

#include "extern/spirv_reflect.h"

DescriptorSetLayout::DescriptorSetLayout(RenderContext& rct, const std::vector<uint8_t>& spvCode)
    : rct_(rct)
{
    autoCreateDSL(spvCode);
}

void DescriptorSetLayout::autoCreateDSL( const std::vector<uint8_t>& spvCode_) {
    // Generate reflection data for a shader
    spv_reflect::ShaderModule module(spvCode_);

    //Enumerate all of Descriptorsets
    uint32_t sc = 0;
    module.EnumerateDescriptorSets(&sc, nullptr);
    setCount = sc;
    std::vector<SpvReflectDescriptorSet*> sets(sc);
    module.EnumerateDescriptorSets(&sc, sets.data());

    std::map<std::pair<uint32_t, uint32_t>, vk::ShaderStageFlags> stageMap;
    for(uint32_t ep = 0 ; ep < module.GetEntryPointCount() ; ++ep){
        auto epName = module.GetEntryPointName(ep);
        auto epStage = module.GetEntryPointShaderStage(ep);  //Vertex or Fragment

        uint32_t bindCount = 0;
        module.EnumerateEntryPointDescriptorBindings(epName, &bindCount, nullptr);
        std::vector<SpvReflectDescriptorBinding*> epBindings(bindCount);
        module.EnumerateEntryPointDescriptorBindings(epName, &bindCount, epBindings.data());

        for(auto* b : epBindings){
            auto key = std::make_pair(b->set, b->binding);
            stageMap[key] |= static_cast<vk::ShaderStageFlags>(epStage);
        }
    }
    // Track descriptor counts per set (set index → descriptor type → count)
    std::map<uint32_t, std::map<vk::DescriptorType, uint32_t>> perSetDescCounts;

    for(auto* set : sets){
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
        vk::DescriptorSetLayoutCreateInfo layoutInfo{};

        for(uint32_t bi = 0 ; bi < set->binding_count ; ++bi){
            auto* b = set->bindings[bi];
            auto key = std::make_pair(b->set, b->binding);

            layoutBindings.emplace_back(
                vk::DescriptorSetLayoutBinding().setBinding(b->binding)
                                                .setDescriptorType(static_cast<vk::DescriptorType>(b->descriptor_type))
                                                .setDescriptorCount(b->count)
                                                .setStageFlags(stageMap[key])
            );
            bindings_.emplace_back(
                ReflectBinding().setBinding(b->binding)
                                .setDescriptorSet(b->set)
                                .setDescriptorType(static_cast<vk::DescriptorType>(b->descriptor_type))
                                .setDescriptorCount(b->count)
                                .setShaderStage(stageMap[key])
                                .setEntryPoint(b->name ? b->name : "")
                                .setBlockSize(b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? b->block.size : 0)
            );

            // Accumulate per-set descriptor type count (unmultiplied)
            perSetDescCounts[b->set][static_cast<vk::DescriptorType>(b->descriptor_type)] += b->count;
        }
        layoutInfo.setBindings(layoutBindings);

        descriptorSetLayouts_.emplace_back(std::move(vk::raii::DescriptorSetLayout(rct_.device, layoutInfo)));
    }
    for(const auto& dsl : descriptorSetLayouts_){
        layoutHandles_.emplace_back(*dsl);
    }

    // Compute pool sizes with per-set multipliers.
    // Set 0: per-frame → × MAX_FRAMES_IN_FLIGHT
    // Set 1+: per-object → × kDefaultObjectMultiplier (upper bound for materials)
    std::map<vk::DescriptorType, uint32_t> totalDescriptors;
    poolMaxSets = 0;

    for(const auto& [setIdx, descCounts] : perSetDescCounts){
        uint32_t multiplier = (setIdx == 0) ? MAX_FRAMES_IN_FLIGHT : DescriptorSetLayout::kDefaultObjectMultiplier;
        poolMaxSets += multiplier;
        for(const auto& [type, count] : descCounts){
            totalDescriptors[type] += count * multiplier;
        }
    }

    poolSizes_.clear();
    for(const auto& [type, count] : totalDescriptors){
        poolSizes_.emplace_back(vk::DescriptorPoolSize().setType(type).setDescriptorCount(count));
    }
}

int DescriptorSetLayout::computePoolMaxSets(uint32_t objectCount) const {
    // Set 0 is per-frame, Set 1+ are per-object
    int total = MAX_FRAMES_IN_FLIGHT;  // Set 0
    for (int i = 1; i < setCount; ++i) {
        total += static_cast<int>(objectCount);
    }
    return total;
}

DescriptorPool::DescriptorPool(RenderContext& rct, int maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes_) : rct_(rct){

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet).setMaxSets(maxSets).setPoolSizes(poolSizes_);

    descriptorPool = vk::raii::DescriptorPool(rct_.device, poolInfo);
}

DescriptorSet::DescriptorSet(RenderContext& rct, const vk::DescriptorSetAllocateInfo& allocInfo_) : rct_(rct) {
    descriptorSets_.clear();
    descriptorSets_ = vk::raii::DescriptorSets(rct_.device, allocInfo_);

    handles_.clear();
    for(auto& ds : descriptorSets_){
        handles_.emplace_back(*ds);
    }
}

PerFrameDescriptorSet::PerFrameDescriptorSet(RenderContext& rct, const vk::DescriptorPool& pool, const vk::DescriptorSetLayout& layoutHandle) {
    sets_.clear();
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layoutHandle);
    vk::DescriptorSetAllocateInfo alloc{};
    alloc.setDescriptorPool(pool)
         .setDescriptorSetCount(MAX_FRAMES_IN_FLIGHT)
         .setSetLayouts(layouts);
    sets_ = vk::raii::DescriptorSets(rct.device, alloc);
    handles_.clear();
    for(auto& set : sets_){
        handles_.emplace_back(*set);
    }
}