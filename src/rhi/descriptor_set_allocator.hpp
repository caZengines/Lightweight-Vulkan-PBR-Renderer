#pragma once
// ============================================================================
// rhi::DescriptorSetAllocator — descriptor pool slabs + descriptor set ownership
//
// Layer: rhi 
//
// Design decisions (see docs/pipeline-descriptor-refactor-plan.md D5):
//
//   * Slabs ("pool chain").  A single VkDescriptorPool's maxSets is frozen at
//     creation and the driver *does* enforce it, so one pool cannot cover a set
//     count that is not known up front.  The allocator therefore keeps a list of
//     slabs, each sized by the same DescriptorPoolBudget, and appends a new one
//     only when every existing slab is full.
//
//   * The allocator OWNS the sets.  Callers get a DescriptorSetId plus a
//     non-owning handle and must return the id via release().  This is what
//     makes per-slab accounting exact — a vk::raii::DescriptorSet handed to a
//     caller frees itself without telling anyone, leaving the allocator unable
//     to observe the release.  Two consequences: accounting stays truthful
//     across load/delete churn, and a slab can never die before a set it handed
//     out (both live inside this object; slots_ is declared after slabs_ so
//     slots are destroyed first, which vkFreeDescriptorSets requires).
//
// Not thread-safe.  Intended to be called from the single-threaded render path.
// ============================================================================

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "rhi/descriptor_binding.hpp"
#include "rhi/descriptor_set_id.hpp"

namespace rhi {

// Per-slab budget.  maxSets is the set limit of ONE slab; the number of slabs is
// managed internally.  Every sizes[i].descriptorCount must be > 0
struct DescriptorPoolBudget {
    size_t                            maxSets = 0;
    std::vector<vk::DescriptorPoolSize> sizes;
};

class DescriptorSetAllocator final {
    public:
        DescriptorSetAllocator(const DescriptorSetAllocator&)            = delete;
        DescriptorSetAllocator& operator=(const DescriptorSetAllocator&) = delete;
        DescriptorSetAllocator(DescriptorSetAllocator&&)                 = delete;
        DescriptorSetAllocator& operator=(DescriptorSetAllocator&&)      = delete;

        // Throws if budget.sizes is empty or any descriptorCount is 0.
        DescriptorSetAllocator(vk::raii::Device& device, const DescriptorPoolBudget& budget);
        ~DescriptorSetAllocator() = default;

        struct AllocatedSet {
            DescriptorSetId   id  = kInvalidDescriptorSet;
            vk::DescriptorSet set = nullptr;   // invalidated by release(id)
        };

        // Throws when a single set can never be satisfied (a binding type absent
        // from the budget, or count > budget.maxSets): growing cannot fix
        // either, and the driver will NOT report a descriptor-count overrun
        [[nodiscard]] AllocatedSet              allocate(
            const vk::DescriptorSetLayout& layout,
            std::span<const DescriptorBinding> bindings);
        [[nodiscard]] std::vector<AllocatedSet> allocate(
            const vk::DescriptorSetLayout& layout,
            std::span<const DescriptorBinding> bindings,
            size_t count);

        // Returns the set and its descriptor quota to the slab it came from, and
        // recycles the id.
        //
        // PRECONDITION (VUID-vkFreeDescriptorSets-pDescriptorSets-00309): every
        // submitted command referring to this set must have finished executing.
        // The render path binds material sets every frame, so callers must defer
        // the release until no frame in flight can still reference it.
        void release(DescriptorSetId id);

        [[nodiscard]] vk::DescriptorSet handle(DescriptorSetId id) const;
        [[nodiscard]] bool             alive(DescriptorSetId id) const noexcept;

        [[nodiscard]] const DescriptorPoolBudget& budget() const noexcept { return budget_; }
        [[nodiscard]] size_t   poolCount()     const noexcept { return slabs_.size(); }
        [[nodiscard]] size_t   liveSets()      const noexcept { return liveSets_; }
        [[nodiscard]] uint64_t highWaterSets() const noexcept { return highWaterSets_; }

    private:
        // NOTE: desc = descriptorSetCount
        struct Slab {
            vk::raii::DescriptorPool pool = nullptr;
            size_t                   setsUsed = 0;   // decremented by release()
            std::vector<uint64_t>    descUsed;       // parallel to budget_.sizes
        };
        struct Slot {
            vk::raii::DescriptorSet set = nullptr;   // allocator-owned
            size_t                  slabIndex = 0;
            std::vector<uint64_t>   descDemand;      // parallel to budget_.sizes
        };

        // Per-type descriptor cost of ONE set built from `bindings`.
        // Throws if a binding type is absent from budget_.sizes.
        [[nodiscard]] std::vector<uint64_t> unitDemand(
            std::span<const DescriptorBinding> bindings) const;

        [[nodiscard]] bool slabFits(const Slab& slab,
                                    std::span<const uint64_t> unit,
                                    uint32_t count) const;

        [[nodiscard]] Slab makeSlab() const;
        void               grow();
        [[nodiscard]] DescriptorSetId acquireId();

        vk::raii::Device*                                device_ = nullptr;
        DescriptorPoolBudget                             budget_;
        std::unordered_map<vk::DescriptorType, size_t> typeIndex_;  // type -> sizes index

        // DECLARATION ORDER IS LOAD-BEARING: slabs_ before slots_, so slots_
        // (each freeing its set into the pool it captured) is destroyed FIRST.
        // Reversing these two produces vkFreeDescriptorSets on a destroyed pool.
        std::vector<Slab>            slabs_;
        std::vector<Slot>            slots_;      // id N lives at slots_[N - 1]
        std::vector<DescriptorSetId> freeIds_;
        DescriptorSetId              nextId_ = 1;
        size_t                       liveSets_ = 0;
        uint64_t                     highWaterSets_ = 0;
};

}  // namespace rhi
