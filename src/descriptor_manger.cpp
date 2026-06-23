#include "descriptor_manager.hpp"
#include "vulkan/vulkan.hpp"
#include <map>
#include <utility>

#include "extern/spirv_reflect.h"

DescriptorSetLayout::DescriptorSetLayout(RenderContext& rct, const std::vector<uint8_t>& spvCode)
    : rct_(rct)
{
    autoCreateDSL(spvCode);
}

void DescriptorSetLayout::autoCreateDSL( const std::vector<uint8_t>& spvCode_){
    // Generate reflection data for a shader
    spv_reflect:: ShaderModule module(spvCode_);

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
    std::map<vk::DescriptorType, int> DescriptorTypes; //used for determining PoolSize
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
            DescriptorTypes[static_cast<vk::DescriptorType>(b->descriptor_type)] += MAX_FRAMES_IN_FLIGHT;
        }
        layoutInfo.setBindings(layoutBindings);

        descriptorSetLayouts.emplace_back(std::move(vk::raii::DescriptorSetLayout(rct_.device, layoutInfo)));
    }
    for(const auto& dsl : descriptorSetLayouts){
        layouthandles.emplace_back(*dsl);
    }
    for(const auto& dt : DescriptorTypes){
        poolSizes.emplace_back(vk::DescriptorPoolSize().setType(dt.first).setDescriptorCount(dt.second));
        poolMaxSets += dt.second;
    }
}

DescriptorPool::DescriptorPool(RenderContext& rct, int maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes_) : rct_(rct){

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet).setMaxSets(maxSets).setPoolSizes(poolSizes_);

    descriptorPool = vk::raii::DescriptorPool(rct_.device, poolInfo);
}

DescriptorSet::DescriptorSet(RenderContext& rct, vk::DescriptorSetAllocateInfo allocInfo_) : rct_(rct){
    descriptorSets.clear();
    descriptorSets = vk::raii::DescriptorSets(rct_.device, allocInfo_);

    handles.clear();
    for(auto& ds : descriptorSets){
        handles.emplace_back(*ds);
    }
}