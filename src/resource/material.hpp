#pragma once
#include "rhi/descriptor_set_id.hpp"
#include "rhi/descriptor_write.hpp"

#include "resource/asset_handle.hpp"
#include "resource/gltf_importer.hpp"   // MaterialData + MaterialTextureSlot (shared vocabulary)
#include "resource/sampler.hpp"
#include "resource/resource_registry.hpp"
#include <array>
#include <glm/glm.hpp>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

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

//It should be noticed that class::Material should not and cannot be copied
class Material{
    public:
        // One asset handle per glTF texture slot, indexed by MaterialTextureSlot.
        using SlotHandles = std::array<resource::AssetHandle, kMaterialSlotCount>;

        // `handles` are asset-library handles (empty = fall back to the registry's
        // built-in default textures, Null Object semantics).  `data` supplies the
        // scalar factors that ride in the push constant block.
        Material(SlotHandles handles,
                 const MaterialData& data,
                 const Sampler& texSampler, const Sampler& norSampler,
                 const resource::ResourceRegistry& registry);

        //ban copy
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&) = default;

        // The two descriptor array values written into set 1
        // (see shaders/raster/shader.slang): slot i of each array is
        // MaterialTextureSlot i.  Binding numbers and descriptor types come from
        // the set layout's table.
        [[nodiscard]] std::vector<rhi::DescriptorWrite> descriptorWrites() const;

        void attachSet(rhi::DescriptorSetId id, vk::DescriptorSet handle);

        [[nodiscard]] const PushConstantBlock& getPushConstantBlock() const { return pcBlock_; }

        [[nodiscard]] vk::DescriptorSet          getDescriptorSet()   const { return descriptorSet_; }
        [[nodiscard]] rhi::DescriptorSetId       descriptorSetId()    const { return descriptorSetId_; }
        [[nodiscard]] bool                       hasDescriptorSet()   const { return descriptorSet_ != nullptr; }

    private:
        PushConstantBlock                      pcBlock_{};

        // Kept so the textures stay loaded (AssetLibrary reference counting).
        SlotHandles                            handles_{};
        vk::Sampler                            texSamplerHandle_;
        vk::Sampler                            norSamplerHandle_;

        // Indexed by MaterialTextureSlot: the single storage location for a slot's
        // descriptor value.  Both arrays are filled by one loop in the constructor.
        std::array<vk::DescriptorImageInfo, kMaterialSlotCount> mapInfos_{};
        std::array<vk::DescriptorImageInfo, kMaterialSlotCount> samplerInfos_{};

        rhi::DescriptorSetId                   descriptorSetId_ = rhi::kInvalidDescriptorSet;
        vk::DescriptorSet                      descriptorSet_   = nullptr;
};

} // namespace resource
