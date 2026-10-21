#pragma once

#include <span>

#include "render/frame/render_item.hpp"
#include "render/pass/pass.hpp"

namespace render {

class GraphicsPipeline;

// One raster pipeline and the items drawn with it.
struct RasterBucket {
    const GraphicsPipeline*     pipeline = nullptr;
    std::span<const RenderItem> items;
};

// Dynamic-rendering graphics pass.
// Depends on render::GraphicsPipeline, render::Material and render::RenderTarget.
class RasterPass final : public Pass {
public:
    explicit RasterPass(std::span<const RasterBucket> buckets);

    void record(const PassContext& ctx) const override;

private:
    std::span<const RasterBucket> buckets_;
};

}  // namespace render
