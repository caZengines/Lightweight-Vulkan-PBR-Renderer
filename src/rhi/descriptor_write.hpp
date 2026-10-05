#pragma once
// ============================================================================
// rhi::DescriptorWrite — "this value belongs to the binding called <name>"
//
// Layer: rhi (0/1).  This is the VALUE side of a descriptor write: one binding
// name plus the descriptor infos that belong to it.  The producer supplies the
// name; the layout table supplies the binding number and descriptor type.
//
// Lifetime: `infos` are views.  They must outlive the writeAll() call that
// consumes them.  Producers normally point at their own members, which do.
// ============================================================================

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

namespace rhi {

struct ImageWrite {
    std::string_view                         name;
    std::span<const vk::DescriptorImageInfo> infos;
};

struct BufferWrite {
    std::string_view                          name;
    std::span<const vk::DescriptorBufferInfo> infos;
};

using DescriptorWrite = std::variant<ImageWrite, BufferWrite>;

[[nodiscard]] inline DescriptorWrite imageWrite(
    std::string_view name, std::span<const vk::DescriptorImageInfo> infos) {
    return ImageWrite{name, infos};
}

[[nodiscard]] inline DescriptorWrite bufferWrite(
    std::string_view name, std::span<const vk::DescriptorBufferInfo> infos) {
    return BufferWrite{name, infos};
}

[[nodiscard]] inline std::string_view nameOf(const DescriptorWrite& write) noexcept {
    return std::visit([](const auto& w) -> std::string_view { return w.name; }, write);
}

// Element count the write supplies; must equal the binding's descriptorCount.
[[nodiscard]] inline size_t countOf(const DescriptorWrite& write) noexcept {
    return std::visit([](const auto& w) -> size_t { return w.infos.size(); }, write);
}

}  // namespace rhi
