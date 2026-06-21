#pragma once
#include "generic/texture.hpp"

class Material{
    public:
        Material(const Texture& albedoTexture, const Texture& normalTexture);

        const vk::DescriptorImageInfo& getImageInfo()  const { return imageInfo; }
        const vk::DescriptorImageInfo& getNormalInfo() const { return normalInfo; }

    private:
        const Texture&            albedoTexture_;
        const Texture&            normalTexture_;
        vk::DescriptorImageInfo   imageInfo;
        vk::DescriptorImageInfo   normalInfo;
};
