#include "render/command_recorder.hpp"

#include "generic/material.hpp"
#include "rhi/rhi_factory.hpp"
#include "render/pipeline.hpp"
#include "rhi/swapchain.hpp"

namespace render {

CommandRecorder::CommandRecorder(const rhi::Swapchain& swapchain,
                                 const Pipeline& pipeline,
                                 const rhi::RhiFactory& factory) noexcept
    : swapchain_(swapchain), pipeline_(pipeline), factory_(factory) {}

void CommandRecorder::record(vk::raii::CommandBuffer& cmd,
                             std::uint32_t imageIndex,
                             const vk::DescriptorSet& frameSet,
                             std::span<const RenderItem> items) {
    cmd.begin({});

    // Swapchain image → color attachment optimal
    factory_.imageBarrier(cmd, swapchain_.Image_.images[imageIndex],
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                          {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);
    // MSAA resolve target → color attachment optimal
    factory_.imageBarrier(cmd, swapchain_.getcolorImage(),
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                          {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);
    // Depth → depth attachment optimal
    factory_.imageBarrier(cmd, swapchain_.getDepthImage(),
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
                          {}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo attachmentInfo;
    attachmentInfo.setImageView(swapchain_.getColorImageView())
                  .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveImageView(swapchain_.Image_.views[imageIndex])
                  .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                  .setLoadOp(vk::AttachmentLoadOp::eClear)
                  .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setClearValue(clearColor);

    vk::RenderingAttachmentInfo depthInfo;
    depthInfo.setImageView(swapchain_.getDepthImageView())
             .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
             .setLoadOp(vk::AttachmentLoadOp::eClear)
             .setStoreOp(vk::AttachmentStoreOp::eDontCare)
             .setClearValue(clearDepth);

    vk::RenderingInfo renderingInfo;
    vk::Rect2D renderArea;
    renderArea.setOffset({0, 0}).setExtent(swapchain_.getExtent());
    renderingInfo.setRenderArea(renderArea)
                 .setLayerCount(1)
                 .setColorAttachments(attachmentInfo)
                 .setPDepthAttachment(&depthInfo);

    cmd.beginRendering(renderingInfo);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_.binding());
        const float w = static_cast<float>(swapchain_.getExtent().width);
        const float h = static_cast<float>(swapchain_.getExtent().height);
        cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, w, h, 0.0f, 1.0f));
        cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_.getExtent()));

        // Set 0: per-frame UBO, shared by all draws of this frame.
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                               *pipeline_.layout(), 0, frameSet, nullptr);

        for (const RenderItem& item : items) {
            if (item.instanceCount == 0) continue;

            // Set 1: per-material textures/samplers.
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   *pipeline_.layout(), 1,
                                   item.material->getDescriptorSet(), nullptr);
            // Push constants: per-material render flags bitmask.
            const std::uint32_t flags = to_uint32(item.material->getFlags());
            cmd.pushConstants<std::uint32_t>(*pipeline_.layout(),
                                             vk::ShaderStageFlagBits::eFragment, 0, flags);

            const std::array<vk::Buffer, 2> vertexBuffers{
                item.mesh->vertexBuffer(),
                item.instanceBuffer};
            constexpr std::array<vk::DeviceSize, 2> offsets{0, 0};
            cmd.bindVertexBuffers(0, vertexBuffers, offsets);
            cmd.bindIndexBuffer(item.mesh->indexBuffer(), 0, vk::IndexType::eUint32);

            cmd.drawIndexed(item.mesh->indexCount(),
                            item.instanceCount,
                            0, 0,
                            item.firstInstance);
        }
    cmd.endRendering();

    // Color attachment → present source
    factory_.imageBarrier(cmd, swapchain_.Image_.images[imageIndex],
                          vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite, {},
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eBottomOfPipe,
                          vk::ImageAspectFlagBits::eColor);

    cmd.end();
}

}  // namespace render
