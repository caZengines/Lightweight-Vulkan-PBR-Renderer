#include "render/descriptor_budget.hpp"

namespace render {

void PoolBudgetBuilder::add(std::span<const vk::DescriptorSetLayoutBinding> table,
                            size_t setCount) {
    if (setCount == 0) return;

    maxSets_ += setCount;
    for (const auto& binding : table) {
        counts_[binding.descriptorType] +=
            static_cast<uint64_t>(binding.descriptorCount) * setCount;
    }
}

rhi::DescriptorPoolBudget PoolBudgetBuilder::build() const {
    rhi::DescriptorPoolBudget budget;
    budget.maxSets = maxSets_;
    budget.sizes.reserve(counts_.size());

    for (const auto& [type, count] : counts_) {
        if (count == 0) continue;   // VUID-VkDescriptorPoolSize-descriptorCount-00302
        vk::DescriptorPoolSize size{};
        size.setType(type).setDescriptorCount(static_cast<uint32_t>(count));
        budget.sizes.emplace_back(size);
    }
    return budget;
}

}  // namespace render
