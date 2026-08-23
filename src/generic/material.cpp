#include "generic/material.hpp"

Material::Material(const resource::AssetHandle& albedo, const resource::AssetHandle& normal,
                   const Sampler& texSampler, const Sampler& norSampler,
                   const resource::ResourceRegistry& registry)
    : albedoHandle_(albedo), normalHandle_(normal),
      texSamplerHandle_(texSampler.getSampler()), norSamplerHandle_(norSampler.getSampler())
{
    // Null Object: a missing/empty handle falls back to the registry's
    // built-in 1×1 default textures.
    const resource::TextureGPU& albedoTex = albedo.valid() ? registry.texture(albedo)
                                                           : registry.defaultAlbedo();
    const resource::TextureGPU& normalTex = normal.valid() ? registry.texture(normal)
                                                           : registry.defaultNormal();
    albedoTexture_ = &albedoTex;
    normalTexture_ = &normalTex;

    imageInfo_.setImageView(albedoTex.view())
              .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    normalInfo_.setImageView(normalTex.view())
               .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    albedoSamplerInfo_.setSampler(texSamplerHandle_);
    normalSamplerInfo_.setSampler(norSamplerHandle_);

    // Build push-constant flags: textures and samplers are always bound
    // Use setFlags() to override per-material at runtime.
    flags_ = RenderFlags::FLAG_ALBEDO_TEXTURE | RenderFlags::FLAG_NORMAL_TEXTURE;
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
