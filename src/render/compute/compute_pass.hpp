#pragma once

#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pass/pass.hpp"

namespace render {

class ComputePipeline;

// Pass-specific compute set; Set0 comes from PassContext.
struct ComputeBindings {
    uint32_t          passSetIndex = 2;
    vk::DescriptorSet passSet = nullptr;
};

// One compute dispatch; must be recorded outside beginRendering/endRendering.
class ComputePass final : public Pass {
public:
    ComputePass(const ComputePipeline& pipeline, ComputeBindings bindings, vk::Extent3D dispatch);

    void record(const PassContext& ctx) const override;

private:
    const ComputePipeline& pipeline_;
    ComputeBindings        bindings_;
    vk::Extent3D           dispatch_;
};

}  // namespace render
