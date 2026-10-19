#pragma once
// ============================================================================
// rhi::DescriptorBinding — one set-layout binding plus its shader-side name
//
// Layer: rhi (0/1).  Pure Vulkan: no reflection types, so rhi never depends on
// resource/ or render/.
//
// vk::DescriptorSetLayoutBinding has no name field; the name is carried in the
// same struct, so reordering a name reorders its binding.
//
// An empty name means "reachable by number only".  That is the case for the
// hand-made set 0, which is written directly by the renderer.
// ============================================================================

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <span>
#include <string>
#include <vector>

namespace rhi {

struct DescriptorBinding {
    vk::DescriptorSetLayoutBinding vk{};
    std::string                    name;
};

// All bindings of ONE set; index into the LayoutSet's vector is the set number.
using DescriptorBindingTable = std::vector<DescriptorBinding>;

// The Vulkan APIs want a contiguous array of the POD part only.  This cannot be
// a span: the two members are interleaved in memory, so no view of the table can
// alias a vk::DescriptorSetLayoutBinding array.
//
// NOTE: callers must keep the result alive across the create call —
// vkCreateDescriptorSetLayout / vkAllocateDescriptorSets only store the pointer.
[[nodiscard]] inline std::vector<vk::DescriptorSetLayoutBinding> vkBindingsOf(std::span<const DescriptorBinding> table) {
    std::vector<vk::DescriptorSetLayoutBinding> out;
    out.reserve(table.size());
    for (const auto& binding : table) out.emplace_back(binding.vk);
    return out;
}

}  // namespace rhi
