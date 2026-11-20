#include "render/pass/pass.hpp"
#include "render/pipeline/pipeline_base.hpp"

namespace render {

void Pass::bindPipelineAndFrameSet(const PassContext &ctx, const PipelineBase &pipeline) {
    ctx.cmd.bindPipeline(pipeline.bindPoint(), pipeline.binding());
    assert(ctx.frameSet);
    ctx.cmd.bindDescriptorSets(pipeline.bindPoint(), pipeline.layout(), 0, ctx.frameSet, nullptr);
}

void Pass::BindCache::bind(const PassContext& ctx, const PipelineBase& pipeline) {
    const vk::PipelineBindPoint bp = pipeline.bindPoint();

    if (&pipeline != lastPipeline_) {
        ctx.cmd.bindPipeline(bp, pipeline.binding());
        lastPipeline_ = &pipeline;
    }

    if (pipeline.layout() != lastLayout_ ||
        ctx.frameSet      != lastFrameSet_ ||
        bp                != lastBindPoint_) {
        assert(ctx.frameSet);
        ctx.cmd.bindDescriptorSets(bp, pipeline.layout(), 0, ctx.frameSet, nullptr);
        lastLayout_    = pipeline.layout();
        lastFrameSet_  = ctx.frameSet;
        lastBindPoint_ = bp;
    }
}

}