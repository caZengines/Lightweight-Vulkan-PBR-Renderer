#pragma once
#include "generic/texture.hpp"
#include "generic/sampler.hpp"
#include "render_context.hpp"


//It should be noticed that 
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
};
