#include "c_engine.hpp"
#include "context.hpp"
#include "descriptor_manager.hpp"
#include "generic/material.hpp"
#include "generic/renderobject.hpp"
#include "platform/log.hpp"
#include "render_context.hpp"
#include "resource/resource_registry.hpp"
#include "resourcefactory.hpp"
#include "vma_allocator.hpp"
#include <chrono>
#include <cstddef>
#include <random>
#include <memory>

CEngine::CEngine(){
    platform::LogLocator::initialize();   // default console logging provider
    initWindow();
    initVulkan();
}

void CEngine::initWindow() {
    platform::WindowConfig config;
    config.width     = 1920;
    config.height    = 1080;
    config.title     = "C' Vulkan";
    config.resizable = true;
    window = std::make_unique<platform::Window>(config);

    // Event hooks
    window->onFramebufferResize = [this](uint32_t /*width*/, uint32_t /*height*/) {
        if (renderer) renderer->framebufferResized = true;
    };
    window->onMouseButton = [this](platform::ButtonAction action, platform::MouseButton button, double x, double y) {
        camera.onMouseButton(button, action, x, y);
    };
    window->onCursorPos = [this](double x, double y) {
        camera.onCursorMove(x, y);
    };
}

void CEngine::initVulkan() {
    VulkanDevice::CreateInfo deviceInfo;
    deviceInfo.requiredDeviceExtensions_ = requiredDeviceExtensions;
    deviceInfo.appName = "No Engine";
    deviceInfo.enableValidationLayers_= enableValidationLayers;
    deviceInfo.instanceExtensions_ = window->requiredInstanceExtensions();
    vulkanDevice_.init(deviceInfo);
    vmaContext_ = std::make_unique<VmaContext>(*vulkanDevice_.physicalDevice, *vulkanDevice_.device, *vulkanDevice_.instance);
    ResourceFactory::init(vulkanDevice_.physicalDevice, vulkanDevice_.device);
    Context::Config cfg;
    cfg.enableValidationLayers_ = enableValidationLayers;
    cfg.validationLayers_       = validationLayers;
    cfg.msaaSamples_            = vulkanDevice_.msaaSamples;
    context = std::make_unique<Context>(cfg, vulkanDevice_.physicalDevice, vulkanDevice_.device, vulkanDevice_.instance, *window);
    createCommandPools();
    initAssetLibrary();
    createSamplers();
    createMaterials();
    createDescriptorSetLayout();
    createDescriptorSetPool();
    initScene();
    initRenderer();
}

void CEngine::run() {
    const auto& batches = scene_.getDrawBatches();
    auto lastTime = std::chrono::high_resolution_clock::now();
    while(!window->shouldClose()) {
        window->pollEvents();
        input.poll(*window);

        const auto now = std::chrono::high_resolution_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        updateCamera(deltaTime);
        renderer->drawFrame(batches);
    }
    vulkanDevice_.device.waitIdle();
}

void CEngine::updateCamera(float deltaTime) {
    //driven by the platform::Input
    if (input.isKeyDown(platform::Key::W))        camera.moveHorizontal(1.0f, 0.0f, deltaTime);
    if (input.isKeyDown(platform::Key::A))        camera.moveHorizontal(0.0f, -1.0f, deltaTime);
    if (input.isKeyDown(platform::Key::S))        camera.moveHorizontal(-1.0f, 0.0f, deltaTime);
    if (input.isKeyDown(platform::Key::D))        camera.moveHorizontal(0.0f, 1.0f, deltaTime);
    if (input.isKeyDown(platform::Key::Space))    camera.moveVertical(1.0f, deltaTime);
    if (input.isKeyDown(platform::Key::LeftShift)) camera.moveVertical(-1.0f, deltaTime);

    const double scroll = input.scrollDelta();
    if (scroll != 0.0) camera.Zoom(scroll);
}

void CEngine::cleanup() {
    if (renderer) {
        renderer->cleanup();
    }
    window.reset();   // destroys the GLFW window and terminates GLFW
}

void CEngine::initAssetLibrary() {
    // Phase 2: AssetLibrary assembly — UploadQueue wraps the transient pool's
    // single-time submissions; ResourceRegistry owns the GPU assets and the
    // built-in default textures; AssetLibrary caches by path with refcounting.
    // (Kept inside the App prototype for now; Phase 5 moves this to app::App.)
    uploadQueue_ = std::make_unique<resource::UploadQueue>(*transientCommandPool);
    resourceRegistry_ = std::make_unique<resource::ResourceRegistry>(vmaContext_->getAllocator(), *uploadQueue_);
    assetLibrary_ = std::make_unique<resource::AssetLibrary>(*resourceRegistry_);
}

void CEngine::createMaterials() {
    // Get-or-load textures; the returned handles are kept by the Materials
    // (refcounted), so duplicate loads never re-upload.
    auto rockAlbedo = assetLibrary_->loadImage(config_.rockTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);
    auto marsAlbedo = assetLibrary_->loadImage(config_.marsTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);

    // Empty (null) normal handle → Material falls back to the registry's
    // built-in flat-normal texture (Null Object semantics).
    defaultMaterial = std::make_shared<Material>(resource::AssetHandle{}, resource::AssetHandle{}, *albedoSampler, *normalSampler, *resourceRegistry_);
    MarsMaterial = std::make_shared<Material>(marsAlbedo, resource::AssetHandle{},
                                              *albedoSampler, *normalSampler, *resourceRegistry_);
    rockMaterial = std::make_shared<Material>(rockAlbedo, resource::AssetHandle{},
                                              *albedoSampler, *normalSampler, *resourceRegistry_);
}

void CEngine::createDescriptorSetLayout() {
    auto spvCode = Pipeline::readFile(config_.shaderPath);
    RenderContext RCT = vulkanDevice_.renderContext();
    descriptorSetLayout = std::make_unique<DescriptorSetLayout>(RCT, spvCode);
}

void CEngine::createDescriptorSetPool() {
    RenderContext RCT = vulkanDevice_.renderContext();
    descriptorPool = std::make_unique<DescriptorPool>(RCT,
                                                      descriptorSetLayout->getPoolMaxSets(), 
                                                      descriptorSetLayout->getPoolSize()
    );
}

 void CEngine::initRenderer(){
     RenderContext RCT = vulkanDevice_.renderContext();
     renderer = std::make_unique<Renderer>(RCT,
         vmaContext_->getAllocator(),
         descriptorSetLayout->getLayoutHandles(),
         *descriptorPool->getDescriptorPool(),
         camera,
         *graphicsCommandPool,
         context->surface,
         *window);
 }

void CEngine::createCommandPools() {
    graphicsCommandPool  = std::make_unique<CommandPool>(vulkanDevice_.device, vulkanDevice_.graphicsQueueIndex, std::move(vulkanDevice_.graphicsQueue), vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    // Transient pool = one-shot uploads (UploadQueue). It must live on the
    // GRAPHICS queue family: mipmap generation records vkCmdBlitImage and
    // fragment-stage barriers, which transfer-only families cannot execute
    // (Phase 2: uploads moved here from the old per-frame graphics pool).
    vk::raii::Queue transientQueue(vulkanDevice_.device.getQueue(vulkanDevice_.graphicsQueueIndex, 0));
    transientCommandPool = std::make_unique<CommandPool>(vulkanDevice_.device, vulkanDevice_.graphicsQueueIndex, std::move(transientQueue),
                                              vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                                                        | vk::CommandPoolCreateFlagBits::eTransient);
}

void CEngine::createSamplers() {
    vk::PhysicalDeviceProperties properties = vulkanDevice_.physicalDevice.getProperties();
    vk::SamplerCreateInfo albedoInfo{};
    albedoInfo.setMagFilter(vk::Filter::eLinear).setMinFilter(vk::Filter::eLinear)
              .setAddressModeU(vk::SamplerAddressMode::eRepeat).setAddressModeV(vk::SamplerAddressMode::eRepeat).setAddressModeW(vk::SamplerAddressMode::eRepeat)
              .setMipmapMode(vk::SamplerMipmapMode::eLinear)
              .setMipLodBias(0.0f).setMaxLod(vk::LodClampNone).setMinLod(0.0f)
              .setAnisotropyEnable(vk::True)
              .setMaxAnisotropy(properties.limits.maxSamplerAnisotropy)
              .setCompareEnable(vk::False).setCompareOp(vk::CompareOp::eAlways)
              .setUnnormalizedCoordinates(vk::False)
              .setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    albedoSampler = std::make_unique<Sampler>(vulkanDevice_.device, albedoInfo);

    vk::SamplerCreateInfo normalInfo{};
    normalInfo.setMagFilter(vk::Filter::eLinear).setMinFilter(vk::Filter::eLinear)
              .setAddressModeU(vk::SamplerAddressMode::eClampToEdge).setAddressModeV(vk::SamplerAddressMode::eClampToEdge).setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
              .setMipmapMode(vk::SamplerMipmapMode::eNearest)
              .setMipLodBias(0.0f).setMaxLod(vk::LodClampNone).setMinLod(0.0f)
              .setAnisotropyEnable(vk::True)
              .setMaxAnisotropy(properties.limits.maxSamplerAnisotropy)
              .setCompareEnable(vk::False).setCompareOp(vk::CompareOp::eAlways)
              .setUnnormalizedCoordinates(vk::False)
              .setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    normalSampler = std::make_unique<Sampler>(vulkanDevice_.device, normalInfo);
}

void CEngine::initScene() {
    RenderContext RCT = vulkanDevice_.renderContext();
    // Get-or-load mesh handles; the RenderObjects keep them alive (refcounted).
    auto marsMeshHandle = assetLibrary_->loadMesh(config_.planetPath);
    std::shared_ptr<RenderObject> mars = std::make_shared<RenderObject>(marsMeshHandle, MarsMaterial, *resourceRegistry_);
    std::vector<InstanceData> instances(1);
    glm::mat4 marsmodel = glm::mat4(1.0f);
    marsmodel = glm::translate(marsmodel, glm::vec3(0.0f, -3.0f, 0.0f));
    marsmodel = glm::scale(marsmodel, glm::vec3(2.0f, 2.0f, 2.0f));
    instances[0].model = marsmodel;
    mars->setInstances(vmaContext_->getAllocator(), instances, *uploadQueue_);
    mars->initMaterialDescriptor(RCT, descriptorSetLayout->getLayoutHandles()[1], *descriptorPool);
    scene_.addObject(std::move(mars));

    auto rockMeshHandle = assetLibrary_->loadMesh(config_.rockPath);
    std::shared_ptr<RenderObject> rock = std::make_shared<RenderObject>(rockMeshHandle, rockMaterial, *resourceRegistry_);
    unsigned int amount = 1000;
    float radius = 40.0;
    float offset = 2.5f;
    std::vector<InstanceData> rocks(amount);
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> radialDist(-offset, offset);
    std::normal_distribution<float> heightDist(0.0f, 2.0f);        // Vertical thickness（Gauss）
    std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);      // rotation angle
    std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);        // random axis
    std::uniform_real_distribution<float> scaleDist(0.05, 0.25); // scale

    std::uniform_real_distribution<float> phaseDist(0.0f, 360.0f);
    for(size_t i = 0 ; i < amount ; ++i){
        glm::mat4 model_ = glm::mat4(1.0f);

        float angle = (360.0f / amount) * i + phaseDist(gen);
        float r = radius + radialDist(gen);      
        float x = sin(glm::radians(angle)) * r;
        float z = cos(glm::radians(angle)) * r;
        float y = heightDist(gen);   
        model_ = glm::translate(model_, glm::vec3(x, y, z));

        glm::vec3 axis = glm::normalize(glm::vec3(
            axisDist(gen),
            axisDist(gen),
            axisDist(gen)
        ));
        float rotAngle = angleDist(gen);
        model_ = glm::rotate(model_, glm::radians(rotAngle), axis);
        float s = scaleDist(gen);
        model_ = glm::scale(model_, glm::vec3(s));

        rocks[i].model = model_;
    }
    rock->setInstances(vmaContext_->getAllocator(), rocks, *uploadQueue_);
    rock->initMaterialDescriptor(RCT, descriptorSetLayout->getLayoutHandles()[1], *descriptorPool);
    scene_.addObject(std::move(rock));
}
