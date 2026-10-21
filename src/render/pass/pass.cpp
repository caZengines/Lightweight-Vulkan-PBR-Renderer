#include "render/pass/pass.hpp"

#include "render/pipeline/pipeline_base.hpp"

#include <cassert>

namespace render {

void Pass::bindPipelineAndFrameSet(const PassContext& ctx, const PipelineBase& pipeline) {
    ctx.cmd.bindPipeline(pipeline.bindPoint(), pipeline.binding());
    assert(ctx.frameSet);
    ctx.cmd.bindDescriptorSets(pipeline.bindPoint(), pipeline.layout(), 0, ctx.frameSet, nullptr);
}

}  // namespace render
