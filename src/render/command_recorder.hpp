#pragma once

#include <cstdint>
#include <span>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/render_item.hpp"

namespace rhi {
class RhiFactory;
class Swapchain;
}  // namespace rhi

namespace render {

class Pipeline;

// Stateless command recording (extracted verbatim from the pre-split
// Renderer::recordCommandBuffer). Binds the graphics pipeline, per-frame
// Set-0, then draws every RenderItem (Set-1 material bind + flags push).
//
// References must outlive this object; Renderer owns the pieces and threads
// them in at construction.
class CommandRecorder final {
public:
    CommandRecorder(const rhi::Swapchain& swapchain,
                    const Pipeline& pipeline,
                    const rhi::RhiFactory& factory) noexcept;

    void record(vk::raii::CommandBuffer& cmd,
                uint32_t imageIndex,
                const vk::DescriptorSet& frameSet,
                std::span<const RenderItem> items);

private:
    const rhi::Swapchain& swapchain_;
    const Pipeline&      pipeline_;
    const rhi::RhiFactory& factory_;
};

}  // namespace render
