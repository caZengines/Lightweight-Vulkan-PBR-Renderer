#include "c_engine.hpp"
#include "generic/material.hpp"
#include "platform/log.hpp"
#include "render_context.hpp"
#include "resource/resource_registry.hpp"

#include <chrono>
#include <memory>
#include <random>

namespace {

// Clamp the requested MSAA count to what the device supports for color
// attachments.
vk::SampleCountFlagBits pickMsaaCount(const vk::raii::PhysicalDevice& physicalDevice,
                                      uint32_t requested) {
    const auto supported =
        physicalDevice.getProperties().limits.framebufferColorSampleCounts;
    for (const uint32_t candidate : {64u, 32u, 16u, 8u, 4u, 2u}) {
        if (requested >= candidate &&
            (supported & static_cast<vk::SampleCountFlagBits>(candidate))) {
            return static_cast<vk::SampleCountFlagBits>(candidate);
        }
    }
    return vk::SampleCountFlagBits::e1;
}

}  // namespace

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
    // Left-drag orbit: raw input plumbing stays at the app boundary; the
    // camera itself is pure math (scene::Camera).
    window->onMouseButton = [this](platform::ButtonAction action, platform::MouseButton button, double x, double y) {
        if (button != platform::MouseButton::Left) return;
        if (action == platform::ButtonAction::Press) {
            orbitDrag_.active = true;
            orbitDrag_.lastX  = x;
            orbitDrag_.lastY  = y;
        } else if (action == platform::ButtonAction::Release) {
            orbitDrag_.active = false;
        }
    };
    window->onCursorPos = [this](double x, double y) {
        if (!orbitDrag_.active) return;
        constexpr float sensitivity = 0.001f;   // legacy Camera default
        const float dx = static_cast<float>(x - orbitDrag_.lastX);
        const float dy = static_cast<float>(y - orbitDrag_.lastY);
        orbitDrag_.lastX = x;
        orbitDrag_.lastY = y;
        camera.orbit(-dx * sensitivity, dy * sensitivity);
    };
}

void CEngine::initVulkan() {
    rhi::VulkanDevice::CreateInfo deviceInfo;
    deviceInfo.requiredDeviceExtensions_ = requiredDeviceExtensions;
    deviceInfo.appName = "No Engine";
    deviceInfo.enableValidationLayers_= enableValidationLayers;
    deviceInfo.instanceExtensions_ = window->requiredInstanceExtensions();
    vulkanDevice_.init(deviceInfo);

    vmaContext_ = std::make_unique<rhi::VmaContext>(*vulkanDevice_.physicalDevice, *vulkanDevice_.device, *vulkanDevice_.instance);
    // Phase 3: factories/managers are constructed once here and injected —
    // no reachable globals left in the rendering path.
    rhiFactory_   = std::make_unique<rhi::RhiFactory>(vulkanDevice_.physicalDevice, vulkanDevice_.device);
    shaderManager_ = std::make_unique<render::ShaderManager>();

    debugMessenger = std::make_unique<rhi::DebugMessenger>(vulkanDevice_.instance,
                                                           enableValidationLayers);
    surface        = std::make_unique<rhi::Surface>(vulkanDevice_.instance, *window);

    createCommandPools();
    initAssetLibrary();
    createSamplers();
    createMaterials();
    createDescriptorSetLayout();
    createDescriptorSetPool();
    initMaterialDescriptors();
    initScene();
    initRenderer();
}

void CEngine::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while(!window->shouldClose()) {
        window->pollEvents();
        input.poll(*window);

        const auto now = std::chrono::high_resolution_clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        updateCamera(deltaTime);

        if (auto frame = renderer->beginFrame()) {
            // Scene produces the draw list (cached; no per-frame allocation).
            renderer->record(*frame, scene_.collectRenderItems());
            renderer->endFrame(*frame);
        }
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
    if (scroll != 0.0) camera.zoom(scroll);
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
    uploadQueue_   = std::make_unique<resource::UploadQueue>(*transientCommandPool, *rhiFactory_,
                                                             vmaContext_->getAllocator());
    resourceRegistry_ = std::make_unique<resource::ResourceRegistry>(vmaContext_->getAllocator(), *uploadQueue_, *rhiFactory_);
    assetLibrary_  = std::make_unique<resource::AssetLibrary>(*resourceRegistry_);
}

void CEngine::createMaterials() {
    // Get-or-load textures; the returned handles are kept by the Materials
    // (refcounted), so duplicate loads never re-upload.
    auto rockAlbedo = assetLibrary_->loadImage(config_.rockTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);
    auto marsAlbedo = assetLibrary_->loadImage(config_.marsTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);

    // Empty (null) normal handle → Material falls back to the registry's
    // built-in default textures (Null Object semantics).
    defaultMaterial = std::make_shared<Material>(resource::AssetHandle{}, resource::AssetHandle{},
                                                 *albedoSampler, *normalSampler, *resourceRegistry_);
    MarsMaterial = std::make_shared<Material>(marsAlbedo, resource::AssetHandle{},
                                              *albedoSampler, *normalSampler, *resourceRegistry_);
    rockMaterial = std::make_shared<Material>(rockAlbedo, resource::AssetHandle{},
                                              *albedoSampler, *normalSampler, *resourceRegistry_);
}

void CEngine::createDescriptorSetLayout() {
    const auto& spvCode = shaderManager_->spirv(config_.shaderPath);
    RenderContext RCT = vulkanDevice_.renderContext();
    descriptorSetLayout = std::make_unique<render::DescriptorSetLayout>(RCT, spvCode);
}

void CEngine::createDescriptorSetPool() {
    RenderContext RCT = vulkanDevice_.renderContext();
    descriptorPool = std::make_unique<render::DescriptorPool>(RCT,
                                                      descriptorSetLayout->getPoolMaxSets(),
                                                      descriptorSetLayout->getPoolSize()
    );
}

void CEngine::initMaterialDescriptors() {
    // One Set-1 per material (not per object): shared materials draw with the
    // same descriptor set no matter how many objects reference them. The set
    // allocation used to live in RenderObject; Phase 4 made scene objects
    // pure data, so the app wires GPU-side materials once, here.
    RenderContext rct = vulkanDevice_.renderContext();
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(descriptorPool->getDescriptorPool())
             .setDescriptorSetCount(1)
             .setSetLayouts(descriptorSetLayout->getLayoutHandles()[1]);
    for (const auto& material : {defaultMaterial, MarsMaterial, rockMaterial}) {
        material->createDescriptorSet(rct, allocInfo);
    }
}

void CEngine::initRenderer(){
    // MSAA: honor Config, clamped to device limits; keep the device-derived
    // contexts in agreement with what the pipeline/attachments will use.
    const vk::SampleCountFlagBits chosenMsaa =
        pickMsaaCount(vulkanDevice_.physicalDevice, config_.msaaSamples);
    vulkanDevice_.msaaSamples = chosenMsaa;
    const render::RenderSettings settings{chosenMsaa,
                                          /*preferredPresentMode*/ vk::PresentModeKHR::eMailbox};

    RenderContext rct = vulkanDevice_.renderContext();  // keep alive for ctor
    render::Renderer::Dependencies deps{
        .rct          = rct,
        .alloc        = vmaContext_->getAllocator(),
        .setLayouts   = descriptorSetLayout->getLayoutHandles(),
        .set0Pool     = *descriptorPool->getDescriptorPool(),
        .graphicsPool = *graphicsCommandPool,
        .camera       = camera,
        .surface      = surface->handle(),
        .window       = *window,
        .spirvPath    = config_.shaderPath,
        .factory      = *rhiFactory_,
    };
    renderer = std::make_unique<render::Renderer>(deps, settings);
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
    // Get-or-load mesh handles; the SceneObjects keep them alive (refcounted).
    auto marsMeshHandle = assetLibrary_->loadMesh(config_.planetPath);
    auto mars = std::make_shared<scene::SceneObject>(marsMeshHandle, MarsMaterial, *resourceRegistry_);
    std::vector<InstanceData> instances(1);
    glm::mat4 marsmodel = glm::mat4(1.0f);
    marsmodel = glm::translate(marsmodel, glm::vec3(0.0f, -3.0f, 0.0f));
    marsmodel = glm::scale(marsmodel, glm::vec3(2.0f, 2.0f, 2.0f));
    instances[0].model = marsmodel;
    mars->setInstances(*uploadQueue_, std::move(instances));
    scene_.addObject(std::move(mars));

    auto rockMeshHandle = assetLibrary_->loadMesh(config_.rockPath);
    auto rock = std::make_shared<scene::SceneObject>(rockMeshHandle, rockMaterial, *resourceRegistry_);
    const uint32_t amount = 1000;
    const float radius = 40.0f;
    const float offset = 2.5f;
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
    rock->setInstances(*uploadQueue_, std::move(rocks));
    scene_.addObject(std::move(rock));
}
