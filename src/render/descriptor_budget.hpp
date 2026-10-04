#pragma once
// ============================================================================
// render::PoolBudgetBuilder — descriptor pool budget derived from the set tables
//
// Layer: render (3).  This is POLICY knowledge (how many sets of which layout
// will be alive at peak), so it stays here; rhi only receives the final numbers.
//
// Why it must be derived rather than written by hand: the local driver does NOT
// enforce VkDescriptorPoolSize::descriptorCount, and the validation layer only
// compares a single request against the pool total without accumulating — so an
// under-declared budget is completely silent locally and only fails on other
// drivers (see docs/pipeline-descriptor-refactor-plan.md D5.1b).
// ============================================================================

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <map>
#include <span>
#include <vector>

#include "rhi/descriptor_set_allocator.hpp"

namespace render {

class PoolBudgetBuilder final {
public:
    // `table` is LayoutSet::bindingTables[i] — the same table the layout was
    // created from and the allocator validates against.  `setCount` is how many
    // sets built from it will be alive at the same time (NOT how many will ever
    // be created: freed sets return their quota to the slab).
    void add(std::span<const vk::DescriptorSetLayoutBinding> table, size_t setCount);

    // Zero-count descriptor types are dropped: VkDescriptorPoolSize requires
    // descriptorCount > 0 (VUID-VkDescriptorPoolSize-descriptorCount-00302).
    [[nodiscard]] rhi::DescriptorPoolBudget build() const;

private:
    size_t                                 maxSets_ = 0;
    std::map<vk::DescriptorType, uint64_t> counts_;
};

}  // namespace render
