#include "resource/sampler.hpp"

Sampler::Sampler(const vk::raii::Device& device_, const vk::SamplerCreateInfo& samplerInfo_) {
    sampler = vk::raii::Sampler(device_, samplerInfo_);
}