#pragma once
// rhi::DescriptorSetId — a stable, reusable handle to a descriptor set owned by
// rhi::DescriptorSetAllocator.  Kept in its own header so consumers (e.g.
// render::Material) can name the handle without pulling in the allocator.
#include <cstddef>

namespace rhi {

using DescriptorSetId = size_t;

// Null Object: no set attached.
inline constexpr DescriptorSetId kInvalidDescriptorSet = 0;

}  // namespace rhi
