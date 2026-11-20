#include "render/pass/raster_pass.hpp"

#include "render/gpu/instance_buffer.hpp"
#include "render/material/material.hpp"
#include "render/pipeline/graphics_pipeline.hpp"
#include "resource/asset/resource_registry.hpp"

#include <array>
#include <cassert>

namespace render {

RasterPass::RasterPass(std::span<const RasterBucket> buckets, const ParticleDraw& particles)
    : buckets_(buckets), particles_(particles) {}

void RasterPass::record(const PassContext& ctx) const {
    assert(ctx.target.valid());

    vk::ClearValue clearColor = vk::ClearColorValue(0.027f, 0.034f, 0.051f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo attachmentInfo;
    attachmentInfo.setImageView(ctx.target.msaaView)
                  .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveImageView(ctx.target.presentView)
                  .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                  .setLoadOp(vk::AttachmentLoadOp::eClear)
                  .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setClearValue(clearColor);

    vk::RenderingAttachmentInfo depthInfo;
    depthInfo.setImageView(ctx.target.depthView)
             .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
             .setLoadOp(vk::AttachmentLoadOp::eClear)
             .setStoreOp(vk::AttachmentStoreOp::eDontCare)
             .setClearValue(clearDepth);

    vk::RenderingInfo renderingInfo;
    vk::Rect2D renderArea;
    renderArea.setOffset({0, 0}).setExtent(ctx.target.extent);
    renderingInfo.setRenderArea(renderArea)
                 .setLayerCount(1)
                 .setColorAttachments(attachmentInfo)
                 .setPDepthAttachment(&depthInfo);

    ctx.cmd.beginRendering(renderingInfo);

    const float width = static_cast<float>(ctx.target.extent.width);
    const float height = static_cast<float>(ctx.target.extent.height);
    ctx.cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f));
    ctx.cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), ctx.target.extent));

    BindCache binder{};
    const Material* boundMaterial = nullptr;

    for (const RasterBucket& bucket : buckets_) {
        if (!bucket.pipeline || bucket.items.empty()) continue;

        binder.bind(ctx, *bucket.pipeline);

        const vk::ShaderStageFlags pushConstantStages = bucket.pipeline->pushConstantStages();

        for (const RenderItem& item : bucket.items) {
            if (item.instanceCount == 0) continue;
            assert(item.mesh && item.material && item.instances);
            if(item.material != boundMaterial) {
                ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                        bucket.pipeline->layout(),
                                        1,
                                        item.material->getDescriptorSet(),
                                        nullptr);
                if (pushConstantStages) {
                    ctx.cmd.pushConstants<render::PushConstantBlock>(
                        bucket.pipeline->layout(), pushConstantStages, 0,
                        item.material->getPushConstantBlock());
                }
                boundMaterial = item.material;
            }

            const std::array<vk::Buffer, 2> vertexBuffers{
                item.mesh->vertexBuffer(),
                item.instances->handle()};
            constexpr std::array<vk::DeviceSize, 2> offsets{0, 0};
            ctx.cmd.bindVertexBuffers(0, vertexBuffers, offsets);
            ctx.cmd.bindIndexBuffer(item.mesh->indexBuffer(), 0, vk::IndexType::eUint32);

            ctx.cmd.drawIndexed(item.mesh->indexCount(),
                                item.instanceCount,
                                0, 0,
                                item.firstInstance);
        }
    }
    if(particles_.pipeline && particles_.vertexCount > 0) {
        binder.bind(ctx, *particles_.pipeline);
        ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   particles_.pipeline->layout(),
                                   2,
                                   particles_.set,
                                   nullptr);
        ctx.cmd.draw(particles_.vertexCount, 1, 0, 0);
    }

    ctx.cmd.endRendering();
}

}  // namespace render
