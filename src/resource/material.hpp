#pragma once
#include "resource/sampler.hpp"
#include "render_context.hpp"
#include "resource/resource_registry.hpp"


// Push-constant render flags
enum class RenderFlags : uint32_t {
    FLAG_ALBEDO_TEXTURE = 1u << 0,   // bit 0: sample albedo texture
    FLAG_NORMAL_TEXTURE = 1u << 1,   // bit 1: sample normal map
};
constexpr uint32_t to_uint32(RenderFlags flags) {
    return static_cast<uint32_t>(flags);
}
constexpr RenderFlags operator|(RenderFlags lhs, RenderFlags rhs) {
    using T = std::underlying_type_t<RenderFlags>;
    return static_cast<RenderFlags>(static_cast<T>(lhs) | static_cast<T>(rhs));
}
constexpr RenderFlags operator&(RenderFlags lhs, RenderFlags rhs) {
    using T = std::underlying_type_t<RenderFlags>;
    return static_cast<RenderFlags>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

//It should be noticed that class::Material should not and cannot be copied
class Material{
    public:
        // albedo/normal: asset handles from AssetLibrary. Empty (null) handles
        // fall back to the registry's built-in default textures (Null Object
        // semantics). Handles are kept so the textures stay loaded.
        Material(const resource::AssetHandle& albedo, const resource::AssetHandle& normal,
                 const Sampler& texSampler, const Sampler& norSampler,
                 const resource::ResourceRegistry& registry);

        //ban copy
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&) = default;

        void createDescriptorSet(RenderContext& rct,
                                 const vk::DescriptorSetAllocateInfo allocInfo_);

        const vk::DescriptorImageInfo& getImageInfo()  const { return imageInfo_; }
        const vk::DescriptorImageInfo& getNormalInfo() const { return normalInfo_; }
        const vk::DescriptorImageInfo& getAlbedoSampler() const { return albedoSamplerInfo_; }
        const vk::DescriptorImageInfo& getNormalSampler() const { return normalSamplerInfo_; }

        const vk::DescriptorSet& getDescriptorSet() const { return *descriptorSet_; }

        RenderFlags getFlags() const { return flags_; }
        void     setFlags(RenderFlags f) { flags_ = f; }

    private:
        resource::AssetHandle                  albedoHandle_;
        resource::AssetHandle                  normalHandle_;
        const resource::TextureGPU*            albedoTexture_ = nullptr;  // registry-owned, kept alive by the handles
        const resource::TextureGPU*            normalTexture_ = nullptr;
        vk::Sampler                            texSamplerHandle_;
        vk::Sampler                            norSamplerHandle_;

        vk::DescriptorImageInfo                imageInfo_{};
        vk::DescriptorImageInfo                normalInfo_{};
        vk::DescriptorImageInfo                albedoSamplerInfo_{};
        vk::DescriptorImageInfo                normalSamplerInfo_{};

        vk::raii::DescriptorSet                descriptorSet_ = nullptr;

        bool                                    descriptorSetCreated_ = false;
        RenderFlags                             flags_;  // computed in constructor
};
