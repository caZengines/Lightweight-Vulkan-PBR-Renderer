#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/render_target.hpp"

namespace rhi {
class RhiFactory;
}  // namespace rhi

namespace render {

// Frame-level command-buffer lifecycle and image layout transitions.
// Depends on rhi::RhiFactory.
class CommandRecorder final {
public:
    explicit CommandRecorder(const rhi::RhiFactory& factory) noexcept;

    void begin(vk::raii::CommandBuffer& cmd, const RenderTarget& target);
    void end(vk::raii::CommandBuffer& cmd, const RenderTarget& target);

private:
    const rhi::RhiFactory& factory_;
};

}  // namespace render
