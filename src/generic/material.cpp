#include "generic/material.hpp"
#include "vulkan/vulkan.hpp"

Material::Material(const std::shared_ptr<const Texture>& albedo, const std::shared_ptr<const Texture>& normal, const Sampler& texSampler, const Sampler& norSampler)
    : albedoTexture_(albedo), normalTexture_(normal), texSamplerHandle_(texSampler.getSampler()), norSamplerHandle_(norSampler.getSampler())
{
    imageInfo_.setImageView(albedoTexture_->getTextureView())
             .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    normalInfo_.setImageView(normalTexture_->getTextureView())
              .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    albedoSamplerInfo_.setSampler(texSamplerHandle_);
    normalSamplerInfo_.setSampler(norSamplerHandle_);
}

void Material::createDescriptorSet(RenderContext& rct, const vk::DescriptorSetAllocateInfo allocInfo_) {
    if(descriptorSetCreated_){
        return;
    }
    auto descriptorSets = rct.device.allocateDescriptorSets(allocInfo_);
    descriptorSet_ = std::move(descriptorSets[0]);
    std::array<vk::WriteDescriptorSet, 4> writes{};
    writes[0].setDstSet(*descriptorSet_).setDstBinding(0)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(imageInfo_);
    writes[1].setDstSet(*descriptorSet_).setDstBinding(1)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(normalInfo_);
    writes[2].setDstSet(*descriptorSet_).setDstBinding(2)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(albedoSamplerInfo_);
    writes[3].setDstSet(*descriptorSet_).setDstBinding(3)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(normalSamplerInfo_);
    rct.device.updateDescriptorSets(writes, {});
    descriptorSetCreated_ = true;
}
