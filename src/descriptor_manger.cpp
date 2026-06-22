#include "descriptor_manager.hpp"
#include "vulkan/vulkan.hpp"
#include <map>

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
    uint32_t setCount = 0;
    module.EnumerateDescriptorSets(&setCount, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(setCount);
    module.EnumerateDescriptorSets(&setCount, sets.data());

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

    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
    std::map<vk::DescriptorType, int> DescriptorTypes; //used for determining PoolSize
    for(auto* set : sets){
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
                                .setDescriptorCount(b->count)
                                .setShaderStage(stageMap[key])
                                .setEntryPoint(b->name ? b->name : "")
                                .setBlockSize(b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? b->block.size : 0)
            );
            DescriptorTypes[static_cast<vk::DescriptorType>(b->descriptor_type)] += MAX_FRAMES_IN_FLIGHT;
        }
    }
    for(const auto& dt : DescriptorTypes){
        poolSizes.emplace_back(vk::DescriptorPoolSize().setType(dt.first)).setDescriptorCount(dt.second);
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.setBindings(layoutBindings);

    descriptorSetLayout = vk::raii::DescriptorSetLayout(rct_.device, layoutInfo);
}

DescriptorPool::DescriptorPool(RenderContext& rct, const std::vector<vk::DescriptorPoolSize>& poolSizes_) : rct_(rct){

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet).setMaxSets(MAX_FRAMES_IN_FLIGHT).setPoolSizes(poolSizes_);

    descriptorPool = vk::raii::DescriptorPool(rct_.device, poolInfo);
}