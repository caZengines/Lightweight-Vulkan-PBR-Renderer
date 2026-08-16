#include "renderer/renderer.hpp"
#include "platform/log.hpp"
#include "render_context.hpp"
#include "vma_allocator.hpp"
#include "vulkan/vulkan.hpp"
#include <cstddef>
#include <memory>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

Renderer::Renderer(RenderContext& rct,
                   VmaAllocator* alloc,
                   const std::vector<vk::DescriptorSetLayout>& dsls,
                   const vk::DescriptorPool& pool,
                   Camera& camera,
                   CommandPool& commandPool,
                   vk::raii::SurfaceKHR& surface,
                   platform::Window& window)
    : window_(window), surface_(surface), rct_(rct), allocator_(alloc), descriptorSetLayouts_(dsls), descriptorPool_(pool), camera_(camera), graphicsCommandPool(commandPool)
{
    swapchainInfo = std::make_unique<Swapchain>(rct_, allocator_, surface, window_);
    perframeDescriptorSet_ = std::make_unique<PerFrameDescriptorSet>(rct_, descriptorPool_, descriptorSetLayouts_[0]);
    createGraphicsPipeline();
    createUniformBuffers();
    createCommandBuffers(graphicsCommandPool);
    createSyncObjects();
}

void Renderer::createGraphicsPipeline() {
    vk::Format                     format = swapchainInfo->getSurfaceFormat().format;
    graphicsPipeline = std::make_unique<Pipeline>(rct_, descriptorSetLayouts_, format);
}

void Renderer::createUniformBuffers() {

    for(size_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT; ++i){
        vk::DeviceSize BufferSize = sizeof(UniformBufferObject);
        vk::BufferCreateInfo bufferCI{};
        bufferCI.setSize(BufferSize).setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
                .setSharingMode(vk::SharingMode::eExclusive);
        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        uniformBuffers_.emplace_back(VmaBuffer(allocator_, static_cast<const VkBufferCreateInfo&>(bufferCI), allocCI));
    }
}

void Renderer::createCommandBuffers(CommandPool& commandPool) {
    vk::CommandBufferAllocateInfo allocInfo;
     allocInfo.setCommandPool(*commandPool.setCommandPool())
              .setLevel(vk::CommandBufferLevel::ePrimary)
              .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
            
    graphicsCommandBuffers = vk::raii::CommandBuffers(rct_.device, allocInfo);
}

void Renderer::createSyncObjects() {
    vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo> SemaphoreType;
    SemaphoreType.get<vk::SemaphoreTypeCreateInfo>().setSemaphoreType(vk::SemaphoreType::eTimeline)
                                                    .setInitialValue(0);
    renderFinishedTimelineSemaphore = vk::raii::Semaphore(rct_.device, SemaphoreType.get<vk::SemaphoreCreateInfo>());
    for(int i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; ++i){
        inFlightFences.emplace_back(rct_.device, vk::FenceCreateInfo(vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled)));
        presentCompleteSemaphores.emplace_back(rct_.device, vk::SemaphoreCreateInfo());
    }
    for(int i = 0 ; i < swapchainInfo->Image_.images.size() ; ++i){
        presentWaitSemaphores.emplace_back(rct_.device, vk::SemaphoreCreateInfo());
    }
}

void Renderer::destroySyncObjects() {
    rct_.device.waitIdle();
    frameCount = 0;
    presentCompleteSemaphores.clear();
    presentWaitSemaphores.clear();
    renderFinishedTimelineSemaphore = nullptr;
    inFlightFences.clear();
}

void Renderer::recreateAfterResize() {
    rct_.device.waitIdle();
    frameCount = 0;
    frameIndex = 0;
    presentCompleteSemaphores.clear();
    presentWaitSemaphores.clear();
    renderFinishedTimelineSemaphore = nullptr;
    inFlightFences.clear();

    swapchainInfo->recreateSwapChain(surface_, window_);
    createSyncObjects();
}

void Renderer::cleanup(){
    rct_.device.waitIdle();
    destroySyncObjects();
    swapchainInfo->cleanupSwapChain();
    cleaned_ = true;
}

void Renderer::drawFrame(const std::vector<DrawBatch>& batches) {
    auto fenceResult = rct_.device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if(fenceResult != vk::Result::eSuccess){
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, imageIndex] = swapchainInfo->swapChain().acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
    if(result == vk::Result::eErrorOutOfDateKHR) {
        recreateAfterResize();
        return;
    }
    else if(result  != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR){
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateUniformBuffer(frameIndex);
    updateDescriptorSet(frameIndex);

    //Only reset the fence if we are submitting work
    rct_.device.resetFences(*inFlightFences[frameIndex]);
    graphicsCommandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex, batches);

    uint64_t signalValue = ++frameCount;
    std::array<vk::Semaphore, 2> signalSemaphores = {
        *renderFinishedTimelineSemaphore,
        *presentWaitSemaphores[imageIndex]
    };
    std::array<uint64_t, 2> signalValues = { signalValue, 0 };

    vk::TimelineSemaphoreSubmitInfo timelineSubmitInfo;
    timelineSubmitInfo.setSignalSemaphoreValueCount(2)
                              .setPSignalSemaphoreValues(signalValues.data());

    //submitting the commandBuffer
    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo = [&]() {
      vk::SubmitInfo info;
      info.setWaitSemaphores(*presentCompleteSemaphores[frameIndex])
          .setWaitDstStageMask(waitDestinationStageMask)
          .setCommandBuffers(*graphicsCommandBuffers[frameIndex])
          .setSignalSemaphores(signalSemaphores)
          .setPNext(&timelineSubmitInfo);
      return info;
    }();
    graphicsCommandPool.queue().submit(submitInfo, *inFlightFences[frameIndex]);

    if(frameCount % 600 == 0){
        glm::vec3 pos = camera_.position();
        platform::LogLocator::get().write(platform::LogLevel::Info,
            "camera position: (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
    }
    const vk::PresentInfoKHR presentInfoKHR = [this, &imageIndex]() {
        vk::PresentInfoKHR info;
        info.setWaitSemaphores(*presentWaitSemaphores[imageIndex])
            .setSwapchains(*swapchainInfo->swapChain())
            .setImageIndices(imageIndex);
        return info;
    }();
    result = graphicsCommandPool.queue().presentKHR(presentInfoKHR);
    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
        framebufferResized = false;
        recreateAfterResize();
    }
    else {
        assert(result == vk::Result::eSuccess);
    }
    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recordCommandBuffer(uint32_t ImageIndex, const std::vector<DrawBatch>& batches) {
    graphicsCommandBuffers[frameIndex].begin({});

    // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
    transition_image_layout(
        swapchainInfo->Image_.images[ImageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                        //srcAccessMask(no need to wait for previous operation)
        vk::AccessFlagBits2::eColorAttachmentWrite,                //dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         //srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,         //dstStage
        vk::ImageAspectFlagBits::eColor
    );
    transition_image_layout(
        swapchainInfo->getcolorImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor
    );
    transition_image_layout(
        swapchainInfo->getDepthImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth
    );
    vk::ClearValue              clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue              clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo attachmentInfo;
    attachmentInfo.setImageView(swapchainInfo->getColorImageView())
                  .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveImageView(swapchainInfo->Image_.ImageViews[ImageIndex])
                  .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                  .setLoadOp(vk::AttachmentLoadOp::eClear)
                  .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setClearValue(clearColor);
    vk::RenderingAttachmentInfo depthInfo;
    depthInfo.setImageView(swapchainInfo->getDepthImageView())
             .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
             .setLoadOp(vk::AttachmentLoadOp::eClear)
             .setStoreOp(vk::AttachmentStoreOp::eDontCare)
             .setClearValue(clearDepth);
    //renderingInfo
    vk::RenderingInfo renderingInfo;
    vk::Rect2D renderArea;
    renderArea.setOffset({0,0})
              .setExtent(swapchainInfo->getExtent());
    renderingInfo.setRenderArea(renderArea)
                 .setLayerCount(1)
                 .setColorAttachments(attachmentInfo)
                 .setPDepthAttachment(&depthInfo);

    //start rendering
    graphicsCommandBuffers[frameIndex].beginRendering(renderingInfo);
        //binding the graphics pipeline
        graphicsCommandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline->binding());
        //command buffer dynamic state
        graphicsCommandBuffers[frameIndex].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchainInfo->getExtent().width),static_cast<float>(swapchainInfo->getExtent().height), 0.0f, 1.0f));
        graphicsCommandBuffers[frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0,0), swapchainInfo->getExtent()));
        // Bind Set 0: per-frame UBO (shared by all draws)
        graphicsCommandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *graphicsPipeline->getLayout(), 0, perframeDescriptorSet_->getHandles()[frameIndex], nullptr);
        // Draw each batch (mesh + material + instances)
        for(const auto& batch : batches){
            if(batch.instanceCount == 0) continue;
            // Bind Set 1: per-material
            graphicsCommandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *graphicsPipeline->getLayout(), 1, batch.material->getDescriptorSet(), nullptr);
            // Push constants: per-material render flags (bitmask)
            uint32_t flags = to_uint32(batch.material->getFlags());
            graphicsCommandBuffers[frameIndex].pushConstants<uint32_t>(
                *graphicsPipeline->getLayout(),
                vk::ShaderStageFlagBits::eFragment,
                0, flags);
            // Bind vertex + instance buffers
            const std::array<vk::Buffer, 2> vertexBuffers {
                batch.mesh->getVertexBuffer(),
                batch.instanceBuffer
            };
            constexpr std::array<vk::DeviceSize, 2> offsets {0, 0};
            graphicsCommandBuffers[frameIndex].bindVertexBuffers(0, vertexBuffers, offsets);
            graphicsCommandBuffers[frameIndex].bindIndexBuffer(batch.mesh->getIndexBuffer(), 0, vk::IndexType::eUint32);

            // Draw all instances of this batch
            graphicsCommandBuffers[frameIndex].drawIndexed(
                static_cast<uint32_t>(batch.mesh->getIndices().size()),
                batch.instanceCount,
                0, 0,
                batch.firstInstance
            );
        }
    //end rendering
    graphicsCommandBuffers[frameIndex].endRendering();
    // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
    transition_image_layout(
        swapchainInfo->Image_.images[ImageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,        // srcAccessMask
        {},                                                // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
        vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
        vk::ImageAspectFlagBits::eColor
    );

    graphicsCommandBuffers[frameIndex].end();
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    glm::vec3 eyePos = camera_.position();

    UniformBufferObject ubo{};
    ubo.view = camera_.viewMatrix();
    ubo.proj =
                glm::perspective(glm::radians(45.0f), static_cast<float>(swapchainInfo->getExtent().width) /static_cast<float>(swapchainInfo->getExtent().height) , 0.1f, 100.0f);
    ubo.proj[1][1] *= -1;

    ubo.camPos = glm::vec4(eyePos, 1);
    ubo.light.pos = glm::vec4(4.0f, 20.0f, -25.0f, 1);
    ubo.light.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.light.intensity = 10.0f;

    memcpy(uniformBuffers_[currentImage].mappedData(), &ubo, sizeof(ubo));
}
void Renderer::updateDescriptorSet(uint32_t currentImage) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.setBuffer(uniformBuffers_[currentImage].getHandle())
              .setOffset(0)
              .setRange(sizeof(UniformBufferObject));

    vk::WriteDescriptorSet write{};
    write.setDstSet(perframeDescriptorSet_->getHandles()[currentImage])
          .setDstBinding(0)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBufferInfo(bufferInfo);
    rct_.device.updateDescriptorSets(write, {});
}

void Renderer::transition_image_layout(
                             VkImage                 image,
                             vk::ImageLayout         old_layout,
                             vk::ImageLayout         new_layout,
                             vk::AccessFlags2        src_access_mask,
                             vk::AccessFlags2        dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask,
                             vk::ImageAspectFlags    image_aspect_flags
                            ) 
{
    vk::ImageMemoryBarrier2 barrier;
    vk::ImageSubresourceRange subresource;
    subresource.setAspectMask(image_aspect_flags)
               .setBaseMipLevel(0)
               .setLevelCount(1)
               .setBaseArrayLayer(0)
               .setLayerCount(1);
    barrier.setSrcStageMask(src_stage_mask)
           .setSrcAccessMask(src_access_mask)
           .setDstStageMask(dst_stage_mask)
           .setDstAccessMask(dst_access_mask)
           .setOldLayout(old_layout)
           .setNewLayout(new_layout)
           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
           .setImage(image)
           .setSubresourceRange(subresource);
           
    vk::DependencyInfo dependency_info;
    dependency_info.setDependencyFlags({})
                   .setImageMemoryBarriers(barrier);
    graphicsCommandBuffers[frameIndex].pipelineBarrier2(dependency_info);
}