#pragma once
#include "generic/texture.hpp"
#include "generic/sampler.hpp"
#include "render_context.hpp"


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
        Material(const std::shared_ptr<const Texture>& albedo, const std::shared_ptr<const Texture>& normal,
                 const Sampler& texSampler, const Sampler& norSampler);

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
        std::shared_ptr<const Texture>          albedoTexture_;
        std::shared_ptr<const Texture>          normalTexture_;
        vk::Sampler                             texSamplerHandle_;
        vk::Sampler                             norSamplerHandle_;

        vk::DescriptorImageInfo                 imageInfo_{};
        vk::DescriptorImageInfo                 normalInfo_{};
        vk::DescriptorImageInfo                 albedoSamplerInfo_{};
        vk::DescriptorImageInfo                 normalSamplerInfo_{};

        vk::raii::DescriptorSet                 descriptorSet_ = nullptr;

        bool                                    descriptorSetCreated_ = false;
        RenderFlags                             flags_;  // computed in constructor
};
