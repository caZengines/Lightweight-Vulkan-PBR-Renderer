#include "render/material/material.hpp"
#include "platform/log.hpp"
#include "resource/asset/resource_registry.hpp"
#include "resource/import/gltf_importer.hpp"  
#include "rhi/descriptor/descriptor_write.hpp"
#include "vulkan/vulkan.hpp"
#include <string>

namespace render {

MaterialPipelineKey makeMaterialPipelineKey(const resource::MaterialData& data) {
    MaterialPipelineKey key = 0;
    for (size_t i = 0; i < resource::kMaterialSlotCount; ++i) {
        const auto& slot = data.slots[i];
        const auto  slotEnum = static_cast<resource::MaterialTextureSlot>(i);
        if(slot.texCoord > 1) {
            platform::LogLocator::get().write(platform::LogLevel::Warning,
                 "There are materials with texcrood greater than 1, which have been ignored by default");
            continue;
        }
        if((slot.texCoord != 1) || (slot.texture < 0)) continue;

        bool isActive = false;
        for(auto info : kActiveMaterialUvSlots) {
            if(info.second == slotEnum) {
                key |= 1u << static_cast<uint32_t>(info.first);
                isActive = true;
                break;
            }
        }
        if(!isActive) {
            platform::LogLocator::get().write(platform::LogLevel::Warning, 
                "[render] MaterialSlot enum" + std::to_string(i) + "non-specialized slot fixed to TEXCOORD_0"
            );
        }
    }
    return key;
}

Material::Material(MaterialId id, SlotHandles handles,
                   const resource::MaterialData& data,
                   const Sampler& texSampler, const Sampler& norSampler,
                   const resource::ResourceRegistry& registry)
    : id_(id),
      handles_(std::move(handles)),
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
    pcBlock_.normalScale     = data.normalScale;
    pcBlock_.occlusionStrength = data.occlusionStrength;
    pcBlock_.alphaMaskCutoff = data.alphaCutoff;
    pcBlock_.flags          |= data.alphaMode == resource::AlphaMode::Mask ? 1u : 0u;

    pipelineKey_ = makeMaterialPipelineKey(data);
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
