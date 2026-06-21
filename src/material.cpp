#include "material.hpp"

Material::Material(const Texture& albedoTexture, const Texture& normalTexture)
    : albedoTexture_(albedoTexture), normalTexture_(normalTexture)
{
    imageInfo.setSampler(albedoTexture_.getTextureSampler())
             .setImageView(albedoTexture_.getTextureView())
             .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    normalInfo.setSampler(normalTexture_.getTextureSampler())
              .setImageView(normalTexture_.getTextureView())
              .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
}
