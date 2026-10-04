#pragma once
// ============================================================================
// rhi::DescriptorWriter — name-based descriptor set writes
//
// Layer: rhi (0/1).  Pure Vulkan: takes vk::DescriptorSetLayoutBinding, not
// reflection types, so rhi never depends on resource/ or render/.
//
// Why names: hand-copied binding NUMBERS are the defect class this replaces
// (material.cpp wrote 10 hardcoded writes and bound the wrong sampler info at
// two of them, silently, because the descriptorType was right).  The caller
// names what it is binding; the number and the type come from the table the set
// layout was built from.
//
// Lifetime: the writer COPIES the info structs into its own storage and points
// the writes at those copies, so a caller cannot create a dangling pImageInfo by
// passing a temporary or a soon-to-die object.
// ============================================================================

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rhi {

class DescriptorWriter final {
public:
    DescriptorWriter(const DescriptorWriter&)            = delete;
    DescriptorWriter& operator=(const DescriptorWriter&) = delete;
    DescriptorWriter(DescriptorWriter&&)                 = delete;
    DescriptorWriter& operator=(DescriptorWriter&&)      = delete;

    // `table` and `names` are parallel and come from the same LayoutSet index
    // (bindingTables[i] / bindingNames[i]). 
    DescriptorWriter(vk::raii::Device& device,
                     vk::DescriptorSet set,
                     std::span<const vk::DescriptorSetLayoutBinding> table,
                     std::span<const std::string> names);

    // Throws when the name is unknown, or when the table's descriptorType does
    // not accept this kind of write.
    DescriptorWriter& writeImage (std::string_view name, const vk::DescriptorImageInfo&);
    DescriptorWriter& writeBuffer(std::string_view name, const vk::DescriptorBufferInfo&);

    // One vkUpdateDescriptorSets.  Throws if a binding was written twice (that is
    // invalid within a single update), or if the writer was already flushed.
    void flush();

    [[nodiscard]] uint32_t writeCount() const noexcept {
        return static_cast<uint32_t>(writes_.size());
    }

private:
    struct Resolved {
        uint32_t           binding = 0;
        vk::DescriptorType type{};
    };

    [[nodiscard]] Resolved resolve(std::string_view name, bool forImage) const;
    uint32_t               reserveBinding(std::string_view name, const Resolved&);

    vk::raii::Device*                               device_ = nullptr;
    vk::DescriptorSet                               set_    = nullptr;
    std::span<const vk::DescriptorSetLayoutBinding> table_;
    std::span<const std::string>                    names_;

    // Stable addresses: the writes point into these.
    std::deque<vk::DescriptorImageInfo>  images_;
    std::deque<vk::DescriptorBufferInfo> buffers_;
    std::vector<vk::WriteDescriptorSet>  writes_;
    std::vector<uint32_t>                writtenBindings_;
    bool                                 flushed_ = false;
};

}  // namespace rhi
