#pragma once

#include "render/material/material_pipeline_key.hpp"
#include "resource/asset/asset_handle.hpp"
#include "resource/gpu/sampler.hpp"
#include "resource/import/gltf_importer.hpp"
#include "rhi/descriptor/descriptor_set_id.hpp"
#include "rhi/descriptor/descriptor_write.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>


#include <vulkan/vulkan_raii.hpp>

namespace resource {

class ResourceRegistry;

} // namespace resource

namespace render {

// Stable handle to a registry-owned material. Ids are dense and follow creation
// order, so a MaterialId doubles as a deterministic draw-list sorting key.
using MaterialId = uint32_t;
inline constexpr MaterialId kInvalidMaterialId = 0xFFFFFFFFu;

class MaterialRegistry;

struct PushConstantBlock {
    glm::vec4 baseColorFactor;            // RGB base color and alpha
    alignas(16)glm::vec4 emissiveFactor;
    float metallicFactor;                 // How metallic the surface is
    float roughnessFactor;                // How rough the surface is
    float normalScale;
    float occlusionStrength;
    float alphaMaskCutoff = 0.5f;         // Alpha threshold for masking
    float clearcoatFactor = 0.0f;
    float clearcoatRoughness = 1.0f;
    uint32_t flags{};                     // Material properties
};
static_assert(sizeof(PushConstantBlock) == 64);
static_assert(offsetof(PushConstantBlock, emissiveFactor) == 16);

// Owned by MaterialRegistry: copying is not allowed and the registry-assigned
// id names the material in the draw list. Pointers and references stay stable
// for the registry's lifetime.
class Material {
    public:
        // One asset handle per glTF texture slot, indexed by MaterialTextureSlot.
        using SlotHandles = std::array<resource::AssetHandle, resource::kMaterialSlotCount>;

        // kInvalidMaterialId until MaterialRegistry assigns the dense id.
        [[nodiscard]] MaterialId id() const noexcept { return id_; }

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

        [[nodiscard]] vk::DescriptorSet          getDescriptorSet()   const noexcept { return descriptorSet_; }
        [[nodiscard]] rhi::DescriptorSetId       descriptorSetId()    const noexcept { return descriptorSetId_; }
        [[nodiscard]] resource::AlphaMode        alphaMode()          const noexcept { return alphaMode_; }
        [[nodiscard]] bool                       isBlend() const noexcept {
            return alphaMode_ == resource::AlphaMode::Blend;
        }
        [[nodiscard]] bool                       hasDescriptorSet()   const { return descriptorSet_ != nullptr; }
        [[nodiscard]] MaterialPipelineKey        pipelineKey()        const noexcept { return pipelineKey_; } 


    private:
        friend class MaterialRegistry;

        // MaterialRegistry is the only creator, so every id matches its
        // creation-order index.
        Material(MaterialId id, SlotHandles handles,
                 const resource::MaterialData& data,
                 const Sampler& texSampler, const Sampler& norSampler,
                 const resource::ResourceRegistry& registry);

        MaterialId                             id_ = kInvalidMaterialId;
        resource::AlphaMode                    alphaMode_ = resource::AlphaMode::Opaque;
        PushConstantBlock                      pcBlock_{};
        MaterialPipelineKey                    pipelineKey_;

        // Kept so the textures stay loaded (AssetLibrary reference counting).
        SlotHandles                            handles_{};
        vk::Sampler                            texSamplerHandle_;
        vk::Sampler                            norSamplerHandle_;

        // Indexed by MaterialTextureSlot: the single storage location for a slot's
        // descriptor value.  Both arrays are filled by one loop in the constructor.
        std::array<vk::DescriptorImageInfo, resource::kMaterialSlotCount> mapInfos_{};
        std::array<vk::DescriptorImageInfo, resource::kMaterialSlotCount> samplerInfos_{};

        rhi::DescriptorSetId                   descriptorSetId_ = rhi::kInvalidDescriptorSet;
        vk::DescriptorSet                      descriptorSet_   = nullptr;
};

} // namespace render
