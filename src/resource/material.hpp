#pragma once
#include "resource/asset_handle.hpp"
#include "resource/sampler.hpp"
#include "render_context.hpp"
#include "resource/resource_registry.hpp"

#include <cstdint>
#include <vector>

//It should be noticed that class::Material should not and cannot be copied
class Material{
    public:
        // albedo/normal: asset handles from AssetLibrary. Empty (null) handles
        // fall back to the registry's built-in default textures (Null Object
        // semantics). Handles are kept so the textures stay loaded.
        Material(const resource::AssetHandle& baseColor, const resource::AssetHandle& metallicRoughness,
                 const resource::AssetHandle& normal, const resource::AssetHandle& occlusion, 
                 const resource::AssetHandle& emissive,
                 const Sampler& texSampler, const Sampler& norSampler,
                 const resource::ResourceRegistry& registry);

        //ban copy
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&) = default;

        void createDescriptorSet(RenderContext& rct,
                                 const vk::DescriptorSetAllocateInfo allocInfo_,
                                 const std::vector<uint32_t>& setBindings);

        const vk::DescriptorImageInfo& getImageInfo()  const { return baseColorInfo_; }
        const vk::DescriptorImageInfo& getNormalInfo() const { return normalInfo_; }
        const vk::DescriptorImageInfo& getbaseColorSampler() const { return baseColorSamplerInfo_; }
        const vk::DescriptorImageInfo& getNormalSampler() const { return normalSamplerInfo_; }

        const vk::DescriptorSet& getDescriptorSet() const { return *descriptorSet_; }

    private:
        resource::AssetHandle                  baseColorHandle_;
        resource::AssetHandle                  metallicRoughnessHandle_;
        resource::AssetHandle                  normalHandle_;
        resource::AssetHandle                  occlusionHandle_;
        resource::AssetHandle                  emissiveHandle_;
        const resource::TextureGPU*            baseColorTexture_         = nullptr;  // registry-owned, kept alive by the handles
        const resource::TextureGPU*            metallicRoughnessTexture_ = nullptr;
        const resource::TextureGPU*            normalTexture_            = nullptr;
        const resource::TextureGPU*            occlusionTexture_         = nullptr;
        const resource::TextureGPU*            emissiveTexture_          = nullptr;
        vk::Sampler                            texSamplerHandle_;
        vk::Sampler                            norSamplerHandle_;

        vk::DescriptorImageInfo                baseColorInfo_{};
        vk::DescriptorImageInfo                rmInfo_{};
        vk::DescriptorImageInfo                normalInfo_{};
        vk::DescriptorImageInfo                aoInfo_{};
        vk::DescriptorImageInfo                emiInfo_{};
        vk::DescriptorImageInfo                baseColorSamplerInfo_{};
        vk::DescriptorImageInfo                normalSamplerInfo_{};

        vk::raii::DescriptorSet                descriptorSet_ = nullptr;

        bool                                   descriptorSetCreated_ = false;
};
