#pragma once
#include "rhi/descriptor_set_id.hpp"

#include "resource/asset_handle.hpp"
#include "resource/sampler.hpp"
#include "resource/resource_registry.hpp"

#include <cstdint>
#include <glm/glm.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace rhi {
class DescriptorWriter;
}  // namespace rhi

namespace resource {

struct PushConstantBlock {
    glm::vec4 baseColorFactor;            // RGB base color and alpha
    alignas(16)glm::vec4 emissiveFactor;
    float metallicFactor;                 // How metallic the surface is
    float roughnessFactor;                // How rough the surface is
    float alphaMask;                      // Whether to use alpha masking
    float alphaMaskCutoff = 0.5f;         // Alpha threshold for masking
};
static_assert(sizeof(PushConstantBlock) == 48);
static_assert(offsetof(PushConstantBlock, emissiveFactor) == 16);

struct MaterialData;

//It should be noticed that class::Material should not and cannot be copied
class Material{
    public:
        // albedo/normal: asset handles from AssetLibrary. Empty (null) handles
        // fall back to the registry's built-in default textures (Null Object
        // semantics). Handles are kept so the textures stay loaded.
        Material(const resource::AssetHandle& baseColor, const resource::AssetHandle& metallicRoughness,
                 const resource::AssetHandle& normal, const resource::AssetHandle& occlusion, 
                 const resource::AssetHandle& emissive,
                 const MaterialData& data,
                 const Sampler& texSampler, const Sampler& norSampler,
                 const resource::ResourceRegistry& registry);

        //ban copy
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&) = default;

        // Describes WHAT to bind, by shader-side name. Does not allocate, does
        // not own: the caller allocated the set and calls flush(). Binding
        // numbers and descriptor types come from the set layout's table, so the
        // numbers are never hand-copied here.
        void bind(rhi::DescriptorWriter& writer) const;

        // Handed back by the assembler after the writes have been flushed.
        // `handle` is non-owning: the allocator owns the set and invalidates it
        // on release.
        void attachSet(rhi::DescriptorSetId id, vk::DescriptorSet handle);

        const PushConstantBlock&       getPushConstantBlock() const { return pcBlock_; }
        const vk::DescriptorImageInfo& getImageInfo()  const { return baseColorInfo_; }
        const vk::DescriptorImageInfo& getNormalInfo() const { return normalInfo_; }
        const vk::DescriptorImageInfo& getbaseColorSampler() const { return baseColorSamplerInfo_; }
        const vk::DescriptorImageInfo& getNormalSampler() const { return normalSamplerInfo_; }

        [[nodiscard]] vk::DescriptorSet          getDescriptorSet()   const { return descriptorSet_; }
        [[nodiscard]] rhi::DescriptorSetId       descriptorSetId()    const { return descriptorSetId_; }
        [[nodiscard]] bool                       hasDescriptorSet()   const { return descriptorSet_ != nullptr; }

    private:
        PushConstantBlock                      pcBlock_{};

        resource::AssetHandle                  baseColorHandle_;
        resource::AssetHandle                  metallicRoughnessHandle_;
        resource::AssetHandle                  normalHandle_;
        resource::AssetHandle                  occlusionHandle_;
        resource::AssetHandle                  emissiveHandle_;
        vk::Sampler                            texSamplerHandle_;
        vk::Sampler                            norSamplerHandle_;

        vk::DescriptorImageInfo                baseColorInfo_{};
        vk::DescriptorImageInfo                rmInfo_{};
        vk::DescriptorImageInfo                normalInfo_{};
        vk::DescriptorImageInfo                aoInfo_{};
        vk::DescriptorImageInfo                emiInfo_{};
        // One per sampler binding: each vk::WriteDescriptorSet points at one of
        // these, so they must stay distinct objects.
        vk::DescriptorImageInfo                baseColorSamplerInfo_{};
        vk::DescriptorImageInfo                mrSamplerInfo_{};
        vk::DescriptorImageInfo                normalSamplerInfo_{};
        vk::DescriptorImageInfo                aoSamplerInfo_{};
        vk::DescriptorImageInfo                emiSamplerInfo_{};

        // Non-owning: rhi::DescriptorSetAllocator owns the set.
        rhi::DescriptorSetId                   descriptorSetId_ = rhi::kInvalidDescriptorSet;
        vk::DescriptorSet                      descriptorSet_   = nullptr;
};

} // namespace resource
