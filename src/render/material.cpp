#include "render/material.hpp"
#include "resource/resource_registry.hpp"
#include "rhi/descriptor_write.hpp"
#include "vulkan/vulkan.hpp"

namespace render {

Material::Material(SlotHandles handles,
                   const resource::MaterialData& data,
                   const Sampler& texSampler, const Sampler& norSampler,
                   const resource::ResourceRegistry& registry)
    : handles_(std::move(handles)),
      texSamplerHandle_(texSampler.getSampler()), norSamplerHandle_(norSampler.getSampler())
{
    // The slot enum is simultaneously the index into the source handles and into
    // both descriptor-value arrays.
    for (size_t i = 0; i < resource::kMaterialSlotCount; ++i) {
        const auto slot = static_cast<resource::MaterialTextureSlot>(i);

        // Null Object: a missing/empty handle falls back to the registry's
        // built-in 1x1 default textures.
        const resource::TextureGPU& texture =
            handles_[i].valid() ? registry.texture(handles_[i])
                                : (slot == resource::MaterialTextureSlot::Normal ? registry.defaultNormal()
                                                                       : registry.defaultAlbedo());

        mapInfos_[i].setImageView(texture.view())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        // Color-ish maps tile → repeat sampler; normal maps must clamp to edge.
        samplerInfos_[i].setSampler(slot == resource::MaterialTextureSlot::Normal ? norSamplerHandle_
                                                                       : texSamplerHandle_);
    }

    pcBlock_.baseColorFactor = data.baseColorFactor;
    pcBlock_.metallicFactor  = data.metallic;
    pcBlock_.roughnessFactor = data.roughness;
    pcBlock_.emissiveFactor  = glm::vec4(data.emissiveFactor, 0.0f);
    pcBlock_.alphaMask       = data.alphaMode == resource::AlphaMode::Mask ? 1.0f : 0.0f;
    pcBlock_.alphaMaskCutoff = data.alphaCutoff;
}

std::vector<rhi::DescriptorWrite> Material::descriptorWrites() const {
    // The shader's two descriptor arrays (shaders/raster/shader.slang).  Slot i of
    // each array is MaterialTextureSlot i, and each array must supply
    // kMaterialSlotCount elements to match the layout's descriptorCount.
    return {
        rhi::imageWrite("maps",     mapInfos_),
        rhi::imageWrite("samplers", samplerInfos_),
    };
}

void Material::attachSet(rhi::DescriptorSetId id, vk::DescriptorSet handle) {
    descriptorSetId_ = id;
    descriptorSet_   = handle;
}

} // namespace render
