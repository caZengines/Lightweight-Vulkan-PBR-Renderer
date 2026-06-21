#include "descriptor_manager.hpp"
#include "renderer.hpp"

#include "extern/spirv_reflect.h"

DescriptorSetLayout::DescriptorSetLayout(RenderContext& rct)
    : rct_(rct)
{
    createDescriptorSetLayout();
}

void DescriptorSetLayout::createDescriptorSetLayout(){
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings{
        vk::DescriptorSetLayoutBinding().setBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding().setBinding(1).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding().setBinding(2).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eFragment)
    };
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.setBindings(bindings);

    descriptorSetLayout = vk::raii::DescriptorSetLayout(rct_.device, layoutInfo);
}

DescriptorPool::DescriptorPool(RenderContext& rct, vk::raii::DescriptorSetLayout& descriptorSetLayout) : rct_(rct){
    std::array<vk::DescriptorPoolSize, 2> poolSize{
        vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize().setType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(MAX_FRAMES_IN_FLIGHT * 2) //AlbedoSampler plus NormalSampler
    };
    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet).setMaxSets(MAX_FRAMES_IN_FLIGHT).setPoolSizes(poolSize);

    descriptorPool = vk::raii::DescriptorPool(rct_.device, poolInfo);
}