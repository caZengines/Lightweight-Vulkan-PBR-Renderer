#pragma once
// ============================================================================
// rhi::DescriptorWriter — table-driven descriptor set writes
//
// Layer: rhi (0/1).  Pure Vulkan: takes rhi::DescriptorBinding (a
// vk::DescriptorSetLayoutBinding plus its reflected name), never reflection
// types, so rhi never depends on resource/ or render/.
//
// Lifetime: the writer COPIES the info structs into its own storage and points
// the writes at those copies, so a producer cannot create a dangling pImageInfo
// by passing a temporary or a soon-to-die object.
// ============================================================================

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <deque>
#include <span>
#include <vector>

#include "rhi/descriptor_binding.hpp"
#include "rhi/descriptor_write.hpp"

namespace rhi {

class DescriptorWriter final {
public:
    DescriptorWriter(const DescriptorWriter&)            = delete;
    DescriptorWriter& operator=(const DescriptorWriter&) = delete;
    DescriptorWriter(DescriptorWriter&&)                 = delete;
    DescriptorWriter& operator=(DescriptorWriter&&)      = delete;

    // `table` owns the set's bindings AND their names; it must outlive the writer.
    DescriptorWriter(vk::raii::Device& device,
                     vk::DescriptorSet set,
                     std::span<const DescriptorBinding> table);

    // Writes EVERY binding of the table, in table order, resolving each one
    // against `values` by name.
    //
    // Throws when a table binding has no value, when a value names a binding the
    // table does not have, when one name is given twice, when the value's kind
    // (image/buffer) disagrees with the descriptor type, or when the element
    // count differs from descriptorCount.
    void writeAll(std::span<const DescriptorWrite> values);

    // One vkUpdateDescriptorSets.  Throws if nothing was written, or if flush()
    // is called twice.
    void flush();

    [[nodiscard]] uint32_t writeCount() const noexcept {
        return static_cast<uint32_t>(writes_.size());
    }

private:
    // Index of the binding called `name`; throws (listing available names) if
    // the table has no such name.  An empty table name never matches.
    [[nodiscard]] size_t indexOfName(std::string_view name) const;

    void apply(const DescriptorBinding& binding, const DescriptorWrite& value);

    vk::raii::Device*                 device_ = nullptr;
    vk::DescriptorSet                 set_    = nullptr;
    std::span<const DescriptorBinding> table_;

    // Stable addresses: the writes point into these.  One vector per write (a
    // deque does not keep its elements contiguous), so array bindings and
    // single bindings take the same code path.
    std::deque<std::vector<vk::DescriptorImageInfo>>  imageArrays_;
    std::deque<std::vector<vk::DescriptorBufferInfo>> bufferArrays_;
    std::vector<vk::WriteDescriptorSet>               writes_;
    bool                                              flushed_ = false;
};

}  // namespace rhi
