#include "render/compute/compute_pass.hpp"

#include "render/compute/compute_pipeline.hpp"

#include <cassert>

namespace render {

ComputePass::ComputePass(const ComputePipeline& pipeline,
                         ComputeBindings bindings,
                         vk::Extent3D dispatch)
    : pipeline_(pipeline), bindings_(bindings), dispatch_(dispatch) {}

void ComputePass::record(const PassContext& ctx) const {
    assert(dispatch_.width > 0 && dispatch_.height > 0 && dispatch_.depth > 0);

    bindPipelineAndFrameSet(ctx, pipeline_);

    if (bindings_.passSet) {
        ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                   pipeline_.layout(),
                                   bindings_.passSetIndex,
                                   bindings_.passSet,
                                   nullptr);
    }

    ctx.cmd.dispatch(dispatch_.width, dispatch_.height, dispatch_.depth);
}

}  // namespace render
