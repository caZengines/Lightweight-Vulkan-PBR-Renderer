#include "resource/material.hpp"
#include "resource/resource_registry.hpp"
#include "resource/gltf_importer.hpp"
#include "rhi/descriptor_writer.hpp"
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

    // Color-ish maps tile → repeat sampler; normal maps must clamp to edge.
    baseColorSamplerInfo_.setSampler(texSamplerHandle_);
    mrSamplerInfo_.setSampler(texSamplerHandle_);
    aoSamplerInfo_.setSampler(texSamplerHandle_);
    emiSamplerInfo_.setSampler(texSamplerHandle_);
    normalSamplerInfo_.setSampler(norSamplerHandle_);

    pcBlock_.baseColorFactor = data.baseColorFactor;
    pcBlock_.metallicFactor  = data.metallic;
    pcBlock_.roughnessFactor = data.roughness;
    pcBlock_.emissiveFactor  = glm::vec4(data.emissiveFactor, 0.0f);
    pcBlock_.alphaMask       = data.alphaMode == AlphaMode::Mask ? 1.0f : 0.0f;
    pcBlock_.alphaMaskCutoff = data.alphaCutoff;
}

void Material::bind(rhi::DescriptorWriter& writer) const {
    // Names are the shader-side contract (shaders/raster/shader.slang).  The
    // binding numbers and descriptor types come from the set layout's table, so
    // they cannot be transcribed wrong here — a typo throws instead.
    writer.writeImage("baseColorMap",         baseColorInfo_)
          .writeImage("baseColorSampler",     baseColorSamplerInfo_)
          .writeImage("metallicRoughnessMap", rmInfo_)
          .writeImage("mrSampler",            mrSamplerInfo_)
          .writeImage("normalMap",            normalInfo_)
          .writeImage("normalSampler",        normalSamplerInfo_)
          .writeImage("occlusionMap",         aoInfo_)
          .writeImage("aoSampler",            aoSamplerInfo_)
          .writeImage("emissiveMap",          emiInfo_)
          .writeImage("emiSampler",           emiSamplerInfo_);
}

void Material::attachSet(rhi::DescriptorSetId id, vk::DescriptorSet handle) {
    descriptorSetId_ = id;
    descriptorSet_   = handle;
}

}
