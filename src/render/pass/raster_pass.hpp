#pragma once

#include <cstdint>
#include <span>

#include "render/frame/render_item.hpp"
#include "render/pass/pass.hpp"
#include <vulkan/vulkan_handles.hpp>

namespace render {

class GraphicsPipeline;

// One raster pipeline and the items drawn with it.
struct RasterBucket {
    const GraphicsPipeline*     pipeline = nullptr;
    std::span<const RenderItem> items;
};

struct ParticleDraw {
    const GraphicsPipeline* pipeline = nullptr;
    vk::DescriptorSet       set      = nullptr;
    uint32_t                vertexCount = 0;
};

// Dynamic-rendering graphics pass.
// Depends on render::GraphicsPipeline, render::Material and render::RenderTarget.
class RasterPass final : public Pass {
public:
    explicit RasterPass(std::span<const RasterBucket> buckets, const ParticleDraw& particles);

    void record(const PassContext& ctx) const override;

private:
    std::span<const RasterBucket> buckets_;
    const ParticleDraw&                 particles_;                                    
};

}  // namespace render
