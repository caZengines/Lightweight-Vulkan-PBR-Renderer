#include "rhi/descriptor_set_allocator.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "platform/log.hpp"

namespace rhi {

namespace {

// A slab is refused once this many have been created: reaching it means the
// budget or the usage pattern is wrong. Allocate up to 8 slabs.
constexpr size_t kMaxSlabs = 8;

[[noreturn]] void fail(const std::string& what) {
    throw std::runtime_error("rhi::DescriptorSetAllocator: " + what);
}

std::string describeType(vk::DescriptorType type) {
    return vk::to_string(type);
}

std::string describeBudget(const DescriptorPoolBudget& budget) {
    std::string out = "maxSets=" + std::to_string(budget.maxSets) + ", sizes{";
    for (size_t i = 0; i < budget.sizes.size(); ++i) {
        if (i != 0) out += ", ";
        out += describeType(budget.sizes[i].type) + "x" +
               std::to_string(budget.sizes[i].descriptorCount);
    }
    return out + "}";
}

}  // namespace

DescriptorSetAllocator::DescriptorSetAllocator(vk::raii::Device& device,
                                               const DescriptorPoolBudget& budget)
    : device_(&device), budget_(budget) {
    if (budget_.sizes.empty()) {
        fail("empty pool budget (no VkDescriptorPoolSize entries)");
    }
    if (budget_.maxSets == 0) {
        fail("pool budget maxSets is 0");
    }
    for (const auto& size : budget_.sizes) {
        if (size.descriptorCount == 0) {
            fail("pool budget declares " + describeType(size.type) +
                 " with descriptorCount 0, which is not a legal VkDescriptorPoolSize");
        }
        typeIndex_.emplace(size.type, typeIndex_.size());
    }

    slabs_.emplace_back(makeSlab());

    platform::LogLocator::get().write(
        platform::LogLevel::Info,
        "[rhi] descriptor slab #1 created (" + describeBudget(budget_) + ")");
}

std::vector<uint64_t> DescriptorSetAllocator::unitDemand(
    std::span<const vk::DescriptorSetLayoutBinding> bindings) const {
    std::vector<uint64_t> demand(budget_.sizes.size(), 0);
    for (const auto& binding : bindings) {
        const auto it = typeIndex_.find(binding.descriptorType);
        if (it == typeIndex_.end()) {
            fail(std::string("binding ") + std::to_string(binding.binding) +
                 " uses " + describeType(binding.descriptorType) +
                 ", which the pool budget never declares (" + describeBudget(budget_) + ")");
        }
        demand[it->second] += binding.descriptorCount;
    }
    return demand;
}

bool DescriptorSetAllocator::slabFits(const Slab& slab, std::span<const uint64_t> unit,
                                      uint32_t count) const {
    if (slab.setsUsed + count > budget_.maxSets) return false;
    for (size_t i = 0; i < unit.size(); ++i) {
        if (slab.descUsed[i] + unit[i] * count > budget_.sizes[i].descriptorCount) return false;
    }
    return true;
}

DescriptorSetAllocator::Slab DescriptorSetAllocator::makeSlab() const {
    std::vector<vk::DescriptorPoolSize> sizes = budget_.sizes;
    vk::DescriptorPoolCreateInfo info{};
    info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(budget_.maxSets)
        .setPoolSizes(sizes);

    Slab slab;
    slab.pool     = vk::raii::DescriptorPool(*device_, info);
    slab.descUsed.assign(budget_.sizes.size(), 0);
    return slab;
}

void DescriptorSetAllocator::grow() {
    if (slabs_.size() >= kMaxSlabs) {
        fail("exhausted " + std::to_string(kMaxSlabs) + " slabs (" + describeBudget(budget_) +
             "); the budget is too small for this usage");
    }
    slabs_.emplace_back(makeSlab());
    platform::LogLocator::get().write(
        platform::LogLevel::Info,
        "[rhi] descriptor slab #" + std::to_string(slabs_.size()) + " created (" +
            describeBudget(budget_) + ")");
}

DescriptorSetId DescriptorSetAllocator::acquireId() {
    if (!freeIds_.empty()) {
        const DescriptorSetId id = freeIds_.back();
        freeIds_.pop_back();
        return id;
    }
    return nextId_++;
}

std::vector<DescriptorSetAllocator::AllocatedSet> DescriptorSetAllocator::allocate(
                                      const vk::DescriptorSetLayout& layout,
                                      std::span<const vk::DescriptorSetLayoutBinding> bindings,
                                      size_t count) {
    if (count == 0) return {};

    const std::vector<uint64_t> unit = unitDemand(bindings);

    // Invariants that no amount of growing can fix, so they are hard errors
    // rather than "try another slab".
    if (count > budget_.maxSets) {
        fail("asked for " + std::to_string(count) + " sets at once, but a slab holds only " +
             std::to_string(budget_.maxSets) + " (" + describeBudget(budget_) + ")");
    }
    for (size_t i = 0; i < unit.size(); ++i) {
        if (unit[i] > budget_.sizes[i].descriptorCount) {
            fail("one set needs " + std::to_string(unit[i]) + "x " +
                 describeType(budget_.sizes[i].type) + " but the budget declares only " +
                 std::to_string(budget_.sizes[i].descriptorCount) + " (" + describeBudget(budget_) +
                 "); the driver will not report this");
        }
    }

    std::vector<vk::DescriptorSetLayout> layouts(count, layout);
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorSetCount(count).setSetLayouts(layouts);

    for (int attempt = 0; attempt < 2; ++attempt) {
        for (size_t i = 0; i < slabs_.size(); ++i) {
            Slab& slab = slabs_[i];
            if (!slabFits(slab, unit, count)) continue;

            allocInfo.setDescriptorPool(*slab.pool);
            std::vector<vk::raii::DescriptorSet> allocated;
            try {
                allocated = device_->allocateDescriptorSets(allocInfo);
            } catch (const vk::SystemError& e) {
                const auto result = static_cast<vk::Result>(e.code().value());
                if (result != vk::Result::eErrorOutOfPoolMemory &&
                    result != vk::Result::eErrorFragmentedPool) {
                    throw;
                }
                // The driver is the authority: our accounting thought this slab
                // had room.  Mark it exactly full and move on (no UINT64_MAX
                // sentinel — it would overflow in slabFits).
                slab.setsUsed = budget_.maxSets;
                for (size_t k = 0; k < slab.descUsed.size(); ++k) {
                    slab.descUsed[k] = budget_.sizes[k].descriptorCount;
                }
                continue;
            }

            std::vector<AllocatedSet> out;
            out.reserve(count);
            for (auto& set : allocated) {
                const DescriptorSetId id = acquireId();
                if (id > slots_.size()) slots_.resize(id);
                Slot& slot    = slots_[id - 1];
                slot.slabIndex = static_cast<uint32_t>(i);
                slot.descDemand = unit;
                slot.set       = std::move(set);
                out.push_back(AllocatedSet{id, *slot.set});
            }

            slab.setsUsed += count;
            for (size_t k = 0; k < unit.size(); ++k) slab.descUsed[k] += unit[k] * count;
            liveSets_ += count;
            highWaterSets_ = std::max<uint64_t>(highWaterSets_, liveSets_);
            return out;
        }
        // Nothing fits: open one more slab and try again.
        grow();
    }

    fail("could not allocate " + std::to_string(count) + " set(s) after growing (" +
         describeBudget(budget_) + ")");
}

DescriptorSetAllocator::AllocatedSet DescriptorSetAllocator::allocate(
    const vk::DescriptorSetLayout& layout,
    std::span<const vk::DescriptorSetLayoutBinding> bindings) {
    return std::move(allocate(layout, bindings, 1).front());
}

void DescriptorSetAllocator::release(DescriptorSetId id) {
    if (!alive(id)) {
        fail("release of descriptor set id " + std::to_string(id) +
             " which is not alive (double release, or a stale id)");
    }

    Slot& slot = slots_[id - 1];
    Slab& slab = slabs_[slot.slabIndex];

    // Frees into the pool this set captured, before the slot is recycled.
    slot.set.clear();

    slab.setsUsed -= 1;
    for (size_t i = 0; i < slot.descDemand.size(); ++i) {
        slab.descUsed[i] -= slot.descDemand[i];
    }

    slot.descDemand.clear();
    slot.slabIndex = 0;

    --liveSets_;
    freeIds_.emplace_back(id);
}

vk::DescriptorSet DescriptorSetAllocator::handle(DescriptorSetId id) const {
    if (!alive(id)) {
        fail("handle() of descriptor set id " + std::to_string(id) + " which is not alive");
    }
    return *slots_[id - 1].set;
}

bool DescriptorSetAllocator::alive(DescriptorSetId id) const noexcept {
    if (id == kInvalidDescriptorSet || id > slots_.size()) return false;
    return static_cast<bool>(*slots_[id - 1].set);
}

}  // namespace rhi
