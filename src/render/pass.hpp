#pragma once

#include <cstdint>
#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/render_item.hpp"
#include "render/render_target.hpp"

namespace render {

class Pipeline;

// Shared per-frame inputs for Pass::record.
struct PassContext {
    vk::raii::CommandBuffer& cmd;
    const RenderTarget&      target;
    vk::DescriptorSet        frameSet = nullptr;
};

// One recorded pass; derived passes borrow per-frame state and own no GPU resources.
class Pass {
public:
    virtual ~Pass() = default;
    virtual void record(const PassContext& ctx) const = 0;

protected:
    // Binds the pipeline and its Set0.
    static void bindPipelineAndFrameSet(const PassContext& ctx, const Pipeline& pipeline);
};

// One pipeline and the items drawn with it.
struct RasterBucket {
    const Pipeline*             pipeline = nullptr;
    std::span<const RenderItem> items;
};

// Dynamic-rendering graphics pass.
// Depends on render::Pipeline, render::Material and render::RenderTarget.
class RasterPass final : public Pass {
public:
    explicit RasterPass(std::span<const RasterBucket> buckets);

    void record(const PassContext& ctx) const override;

private:
    std::span<const RasterBucket> buckets_;
};

// Pass-specific compute set; Set0 comes from PassContext.
struct ComputeBindings {
    uint32_t          passSetIndex = 2;
    vk::DescriptorSet passSet = nullptr;
};

// One compute dispatch; must be recorded outside beginRendering/endRendering.
class ComputePass final : public Pass {
public:
    ComputePass(const Pipeline& pipeline, ComputeBindings bindings, vk::Extent3D dispatch);

    void record(const PassContext& ctx) const override;

private:
    const Pipeline& pipeline_;
    ComputeBindings  bindings_;
    vk::Extent3D     dispatch_;
};

}  // namespace render
