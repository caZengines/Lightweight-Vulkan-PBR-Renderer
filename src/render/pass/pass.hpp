#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/frame/render_target.hpp"

namespace render {

class PipelineBase;

// Shared per-frame inputs for Pass::record.
struct PassContext {
    vk::raii::CommandBuffer& cmd;
    const RenderTarget&      target;
    vk::DescriptorSet        frameSet = nullptr;
};

// One recorded pass; derived passes borrow per-frame state and own no GPU resources.
// Raster work lives in RasterPass, dispatch work in ComputePass.
class Pass {
public:
    virtual ~Pass() = default;
    virtual void record(const PassContext& ctx) const = 0;

protected:
    // Binds the pipeline and its Set0.
    static void bindPipelineAndFrameSet(const PassContext& ctx, const PipelineBase& pipeline);
};

}  // namespace render
