#pragma once

#include <span>

#include "render/frame/render_item.hpp"

namespace scene {


// Reorders render items into batched draw order so the renderer binds pipeline
// and material state once per run instead of once per item: items sharing a
// MaterialPipelineKey become contiguous (largest groups first), and within such
// a group items sharing a material id become contiguous the same way.
// Equal-sized groups keep first-appearance order and items keep their relative
// order inside a group.
//
// Blend items are not batched: they are only moved behind the opaque items so
// the renderer can depth-sort the individual transparent instances per frame.
//
// The scene applies this after all content is added, so batching spans every
// model. Reordering is a permutation of the input array (moves only): no
// geometry or material is copied.
void batchRenderItems(std::span<render::RenderItem> items);

}  // namespace scene
