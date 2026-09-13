#include "resource/material.hpp"
#include "resource/resource_registry.hpp"
#include "resource/gltf_importer.hpp"
#include "vulkan/vulkan.hpp"

namespace resource {

struct MaterialData;

Material::Material(const resource::AssetHandle& baseColor, const resource::AssetHandle& metallicRoughness,
                   const resource::AssetHandle& normal, const resource::AssetHandle& occlusion, 
                   const resource::AssetHandle& emissive,
                   const MaterialData& data,
                   const Sampler& texSampler, const Sampler& norSampler,
                   const resource::ResourceRegistry& registry)
    : baseColorHandle_(baseColor), metallicRoughnessHandle_(metallicRoughness), normalHandle_(normal),
      occlusionHandle_(occlusion), emissiveHandle_(emissive),
      texSamplerHandle_(texSampler.getSampler()), norSamplerHandle_(norSampler.getSampler())
{
    // Null Object: a missing/empty handle falls back to the registry's
    // built-in 1×1 default textures.
    const resource::TextureGPU& baseColorTex         = baseColor.valid() ? registry.texture(baseColor)
                                                           : registry.defaultAlbedo();
    const resource::TextureGPU& metallicRoughnessTex = metallicRoughness.valid() ? registry.texture(metallicRoughness)
                                                           : registry.defaultAlbedo();
    const resource::TextureGPU& normalTex            = normal.valid() ? registry.texture(normal)
                                                           : registry.defaultNormal();
    const resource::TextureGPU& occlusionTex         = occlusion.valid() ? registry.texture(occlusion)
                                                           : registry.defaultAlbedo();
    const resource::TextureGPU& emissiveTex          = emissive.valid() ? registry.texture(emissive)
                                                           : registry.defaultAlbedo();
    baseColorTexture_ = &baseColorTex;
    normalTexture_   = &normalTex;

    baseColorInfo_.setImageView(baseColorTex.view())
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    rmInfo_.setImageView(metallicRoughnessTex.view())
           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    normalInfo_.setImageView(normalTex.view())
               .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    aoInfo_.setImageView(occlusionTex.view())
           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    emiInfo_.setImageView(emissiveTex.view())
           .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    baseColorSamplerInfo_.setSampler(texSamplerHandle_);
    normalSamplerInfo_.setSampler(norSamplerHandle_);

    pcBlock_.baseColorFactor = data.baseColorFactor;
    pcBlock_.metallicFactor  = data.metallic;
    pcBlock_.roughnessFactor = data.roughness;
    pcBlock_.alphaMask       = data.alphaMode == AlphaMode::Mask ? 1.0f : 0.0f;
    pcBlock_.alphaMaskCutoff = data.alphaCutoff;
}

void Material::createDescriptorSet(RenderContext& rct, const vk::DescriptorSetAllocateInfo allocInfo_,
                                   const std::vector<uint32_t>& setBindings) {
    if(descriptorSetCreated_){
        return;
    }
    auto descriptorSets = rct.device.allocateDescriptorSets(allocInfo_);
    descriptorSet_ = std::move(descriptorSets[0]);
    std::array<vk::WriteDescriptorSet, 10> writes{};
    writes[0].setDstSet(*descriptorSet_).setDstBinding(0)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(baseColorInfo_);
    writes[1].setDstSet(*descriptorSet_).setDstBinding(1)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(baseColorSamplerInfo_);
    writes[2].setDstSet(*descriptorSet_).setDstBinding(2)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(rmInfo_);
    writes[3].setDstSet(*descriptorSet_).setDstBinding(3)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(normalSamplerInfo_);
    writes[4].setDstSet(*descriptorSet_).setDstBinding(4)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(normalInfo_);
    writes[5].setDstSet(*descriptorSet_).setDstBinding(5)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(normalSamplerInfo_);
    writes[6].setDstSet(*descriptorSet_).setDstBinding(6)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(aoInfo_);
    writes[7].setDstSet(*descriptorSet_).setDstBinding(7)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(normalSamplerInfo_);
    writes[8].setDstSet(*descriptorSet_).setDstBinding(8)
             .setDescriptorType(vk::DescriptorType::eSampledImage)
             .setImageInfo(emiInfo_);
    writes[9].setDstSet(*descriptorSet_).setDstBinding(9)
             .setDescriptorType(vk::DescriptorType::eSampler)
             .setImageInfo(baseColorSamplerInfo_);

    // The set layout comes from shader reflection, so only bindings the
    // shader actually declares may receive a write.
    auto inLayout = [&setBindings](uint32_t binding) {
        for (const uint32_t b : setBindings) {
            if (b == binding) return true;
        }
        return false;
    };
    uint32_t writeCount = 0;
    for (const auto& w : writes) {
        if (inLayout(w.dstBinding)) ++writeCount;
    }
    std::vector<vk::WriteDescriptorSet> activeWrites;
    activeWrites.reserve(writeCount);
    for (const auto& w : writes) {
        if (inLayout(w.dstBinding)) {
            activeWrites.emplace_back(w);
        }
    }
    rct.device.updateDescriptorSets(activeWrites, {});
    descriptorSetCreated_ = true;
}

}
