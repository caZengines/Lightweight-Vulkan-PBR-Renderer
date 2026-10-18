#include "render/command_recorder.hpp"

#include "rhi/rhi_factory.hpp"
#include "vulkan/vulkan.hpp"

#include <cassert>

namespace render {

CommandRecorder::CommandRecorder(const rhi::RhiFactory& factory) noexcept
    : factory_(factory) {}

void CommandRecorder::begin(vk::raii::CommandBuffer& cmd, const RenderTarget& target) {
    assert(target.valid());
    cmd.begin({});

    factory_.imageBarrier(cmd, target.presentImage,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);

    factory_.imageBarrier(cmd, target.msaaImage,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);

    factory_.imageBarrier(cmd, target.depthImage,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          {},
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth);
}

void CommandRecorder::end(vk::raii::CommandBuffer& cmd, const RenderTarget& target) {
    assert(target.valid());

    factory_.imageBarrier(cmd, target.presentImage,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          {},
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eBottomOfPipe,
                          vk::ImageAspectFlagBits::eColor);

    cmd.end();
}

}  // namespace render
