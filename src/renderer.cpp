#include "renderer.hpp"
#include "command_manager.hpp"
#include "descriptor_manager.hpp"
#include "generic/vertex.hpp"
#include "render_context.hpp"
#include "swapchain.hpp"
#include "resourcefactory.hpp"
#include "vulkan/vulkan.hpp"
#include <cstddef>
#include <memory>
#include <random>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

Renderer::Renderer(RenderContext& rct, Material& material, obj_Model& model, Camera& camera, CommandPool& commandPool, vk::raii::SurfaceKHR& surface, GLFWwindow* window)
    : rct_(rct), material_(material), model_(model), camera_(camera), graphicsCommandPool(commandPool), surface_(surface), window_(window)
{
    swapchainInfo = std::make_unique<Swapchain>(rct_, surface, window_);
    createDescriptorSetLayout();
    initInstanceDatas();
    createInstanceBuffer();
    createGraphicsPipeline();
    createUniformBuffers();
    createDescriptorPoolAndSets();
    createCommandBuffers(graphicsCommandPool);
    createSyncObjects();
}

void Renderer::initInstanceDatas(){
    InstanceData instanceData{};
    instanceDatas.reserve(instanceText);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-2.0, 2.0);
    glm::mat4 rotation = glm::rotate(
        glm::mat4(1.0f), 
        glm::radians(180.0f), 
        glm::vec3(1.0f, 0.0f, 0.0f)
    );
    for(size_t i = 0 ; i < instanceText ; ++i){
        instanceData.enableNormal = 1;
        float x = dist(gen);
        float z = dist(gen);
        float y = dist(gen);
        glm::vec3 translation(x, y, z);
        instanceData.model = glm::translate(glm::mat4(1.0f), translation) * rotation;
        instanceDatas.emplace_back(instanceData);
    }
}

void Renderer::createInstanceBuffer(){
    Buffer<InstanceData>::CreateInfo instanceInfo;
    instanceInfo.size = sizeof(InstanceData) * instanceDatas.size();
    instanceInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
    instanceInfo.memProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    instanceBuffer = std::make_unique<Buffer<InstanceData>>(instanceDatas, instanceInfo, graphicsCommandPool);
}

void Renderer::createGraphicsPipeline(){
    vk::Format                     format = swapchainInfo->getSurfaceFormat().format;
    graphicsPipeline = std::make_unique<Pipeline>(rct_, descriptorSetLayout->getLayoutHandles(), format);
}

void Renderer::createDescriptorSetLayout(){
    auto spvCode = Pipeline::readFile("../shaders/slang.spv");
    descriptorSetLayout = std::make_unique<DescriptorSetLayout>(rct_, spvCode);
}

void Renderer::createDescriptorPoolAndSets(){
    descriptorPool = std::make_unique<DescriptorPool>(rct_,
                                                      descriptorSetLayout->getPoolMaxSets(), 
                                                      descriptorSetLayout->getPoolSize()
    );
    //create DescriptorSets
    descriptorSets.reserve(MAX_FRAMES_IN_FLIGHT);

    for(size_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; ++i){
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.setDescriptorPool(descriptorPool->getDescriptorPool())
                 .setDescriptorSetCount(descriptorSetLayout->getSetCount())
                 .setSetLayouts(descriptorSetLayout->getLayoutHandles());
        descriptorSets.emplace_back(std::make_unique<DescriptorSet>(rct_, allocInfo));
    }
}

void Renderer::createUniformBuffers(){
    auto& factory = ResourceFactory::get();

    for(size_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT; ++i){
        vk::DeviceSize BufferSize = sizeof(UniformBufferObject);
        auto [buffer, bufferMem] = 
                factory.createBuffer(
                    BufferSize,
                    vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        uniformBuffers.emplace_back(std::move(buffer));
        uniformBuffersMemory.emplace_back(std::move(bufferMem));
        uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, BufferSize));
    }
}

void Renderer::createCommandBuffers(CommandPool& commandPool){
    vk::CommandBufferAllocateInfo allocInfo;
     allocInfo.setCommandPool(*commandPool.setCommandPool())
              .setLevel(vk::CommandBufferLevel::ePrimary)
              .setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
            
    graphicsCommandBuffers = vk::raii::CommandBuffers(rct_.device, allocInfo);
}

void Renderer::createSyncObjects(){
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

void Renderer::destroySyncObjects(){
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

void Renderer::drawFrame(){
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
    recordCommandBuffer(imageIndex);

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

void Renderer::recordCommandBuffer(uint32_t ImageIndex){
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
        *swapchainInfo->getcolorImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor
    );
    transition_image_layout(
        *swapchainInfo->getDepthImage(),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
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
        //binding the vertexbuffer & instanceBuffer
        const std::array<vk::Buffer, 2> vertexBuffers {*model_.getVertexBuffer(), instanceBuffer->getBuffer()};
        constexpr std::array<vk::DeviceSize, 2> offsets {0, 0};
        graphicsCommandBuffers[frameIndex].bindVertexBuffers(0, vertexBuffers, offsets);
        graphicsCommandBuffers[frameIndex].bindIndexBuffer(*model_.getIndexBuffer(), 0, vk::IndexType::eUint32);
        //command buffer dynamic state
        graphicsCommandBuffers[frameIndex].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchainInfo->getExtent().width),static_cast<float>(swapchainInfo->getExtent().height), 0.0f, 1.0f));
        graphicsCommandBuffers[frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0,0), swapchainInfo->getExtent()));
        //binding descriptorSets
        graphicsCommandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *graphicsPipeline->getLayout(), 0, descriptorSets[frameIndex]->getSetsHandles(), nullptr);
        //record draw command
        graphicsCommandBuffers[frameIndex].drawIndexed(static_cast<uint32_t>(model_.getIndices().size()), instanceDatas.size(), 0, 0, 0);
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

void Renderer::updateUniformBuffer(uint32_t currentImage){
    glm::vec3 eyePos = camera_.position();


    UniformBufferObject ubo{};
    ubo.view = lookAt(eyePos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj =
                 glm::perspective(glm::radians(45.0f), static_cast<float>(swapchainInfo->getExtent().width) /static_cast<float>(swapchainInfo->getExtent().height) , 0.1f, 100.0f);
    ubo.proj[1][1] *= -1;

    ubo.camPos = glm::vec4(eyePos, 1);
    ubo.light.pos = glm::vec4(1.0f, 3.0f, 3.0f, 1);
    ubo.light.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.light.intensity = 0.6f;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::updateDescriptorSet(uint32_t currentImage){
    const auto& descriptorSets_ = descriptorSets[currentImage]->getDescriptorSets();
    //Temporarily hard coded
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(uniformBuffers[currentImage]).setOffset(0).setRange(sizeof(UniformBufferObject));

    const auto& bindings_ = descriptorSetLayout->getBindings();
    std::vector<vk::WriteDescriptorSet> writes; writes.reserve(bindings_.size());
    for(const auto& b : bindings_){
        vk::WriteDescriptorSet write;
        write.setDstSet(*descriptorSets_[b.set]).setDstBinding(b.binding)
             .setDescriptorType(b.descriptorType)
             .setDescriptorCount(b.count);
        if(b.descriptorType == vk::DescriptorType::eUniformBuffer){
            write.setBufferInfo(bufferInfo);
        }
        else if(b.binding == Binding::kAlbedoTexture){
            write.setImageInfo(material_.getImageInfo());
        }
        else if(b.binding == Binding::kNormalTexure) write.setImageInfo(material_.getNormalInfo());

        writes.emplace_back(write);
    }
    rct_.device.updateDescriptorSets(writes, {});
}

void Renderer::transition_image_layout(
                             vk::Image               image,
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