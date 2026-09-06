#include "app/app.hpp"

#include "app/action_context.hpp"
#include "app/game_loop.hpp"
#include "platform/input.hpp"
#include "rhi/command_pool.hpp"
#include "resource/material.hpp"
#include "resource/sampler.hpp"
#include "platform/log.hpp"
#include "render_context.hpp"
#include "resource/resource_registry.hpp"

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

namespace app {

App::App() {
    platform::LogLocator::initialize();   // default console logging provider
    initWindow();
    initRhi();
    initActionContext();
    initResources();
    initSamplers();
    initContent();
    initRender();
}

App::~App() {
    if (scene_.isValid()) {
        scene_.clear();
    }
    if (renderer_) {
        renderer_->cleanup();
    }
    window_.reset();   // destroys the GLFW window and terminates GLFW
}

void App::run() {
    GameLoop loop(*window_, input_);
    loop.setUpdate([this](float deltaTime) {
        actions_.update(input_);
        cameraController_.update(actions_, input_, deltaTime);
    });
    loop.setFrame([this] {
        if (auto frame = renderer_->beginFrame()) {
            // Scene produces the draw list (cached; no per-frame allocation).
            renderer_->record(*frame, scene_.collectRenderItems());
            renderer_->endFrame(*frame);
            return true;
        }
        return false;
    });
    loop.run();
    vulkanDevice_.device.waitIdle();
}

void App::initWindow() {
    platform::WindowConfig windowConfig;
    windowConfig.width     = config_.width;
    windowConfig.height    = config_.height;
    windowConfig.title     = config_.title.c_str();
    windowConfig.resizable = config_.resizable;
    window_ = std::make_unique<platform::Window>(windowConfig);
    cameraController_.setWindow(*window_);   // title feedback for camera hotkeys

    // Event hooks → input mapping / renderer notification.
    window_->onFramebufferResize = [this](uint32_t /*width*/, uint32_t /*height*/) {
        if (renderer_) renderer_->framebufferResized = true;
    };
}

void App::initRhi() {
    rhi::VulkanDevice::CreateInfo deviceInfo;
    deviceInfo.requiredDeviceExtensions_ = config_.requiredDeviceExtensions;
    deviceInfo.appName = "No Engine";
    deviceInfo.enableValidationLayers_ = config_.enableValidationLayers;
    deviceInfo.instanceExtensions_ = window_->requiredInstanceExtensions();
    vulkanDevice_.init(deviceInfo);

    vmaContext_ = std::make_unique<rhi::VmaContext>(*vulkanDevice_.physicalDevice,
                                                    *vulkanDevice_.device,
                                                    *vulkanDevice_.instance);
    // Factories/managers are constructed once here and injected — no reachable
    // globals left in the rendering path.
    rhiFactory_    = std::make_unique<rhi::RhiFactory>(vulkanDevice_.physicalDevice, vulkanDevice_.device);
    shaderManager_ = std::make_unique<render::ShaderManager>();

    debugMessenger_ = std::make_unique<rhi::DebugMessenger>(vulkanDevice_.instance,
                                                            config_.enableValidationLayers);
    surface_        = std::make_unique<rhi::Surface>(vulkanDevice_.instance, *window_);

    graphicsCommandPool_ = std::make_unique<rhi::CommandPool>(
        vulkanDevice_.device, vulkanDevice_.graphicsQueueIndex,
        std::move(vulkanDevice_.graphicsQueue), vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    // Transient pool = one-shot uploads (UploadQueue). It must live on the
    // GRAPHICS queue family: mipmap generation records vkCmdBlitImage and
    // fragment-stage barriers, which transfer-only families cannot execute.
    vk::raii::Queue transientQueue(vulkanDevice_.device.getQueue(vulkanDevice_.graphicsQueueIndex, 0));
    transientCommandPool_ = std::make_unique<rhi::CommandPool>(
        vulkanDevice_.device, vulkanDevice_.graphicsQueueIndex, std::move(transientQueue),
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer
        | vk::CommandPoolCreateFlagBits::eTransient);
}

void App::initActionContext() {
    using K = platform::Key;  using MB = platform::MouseButton;
    using F = ModifierFlag;   using A = Action;
    using T = Trigger;        using M = Mode;
    actions_.bind(MB::Right,  F::Shift, A::toggleCameraControlMode, T::Hold, M::Default);
    actions_.bind(MB::Middle, F::Shift, A::cameraPan, T::Hold, M::Default);
    actions_.bind(MB::Middle, A::OrbitalRotation, T::Hold, M::Default);
    actions_.bind(K::KP5, A::toggleProjection, T::Press, M::Default);

    // Roam movement — the action layer gates these to the walk gesture.
    actions_.bind(K::W, A::moveForward,  T::Hold, M::Walk);
    actions_.bind(K::A, A::moveLeft,     T::Hold, M::Walk);
    actions_.bind(K::S, A::moveBackward, T::Hold, M::Walk);
    actions_.bind(K::D, A::moveRight,    T::Hold, M::Walk);
    actions_.bind(K::Q, A::moveUp,       T::Hold, M::Walk);
    actions_.bind(K::E, A::moveDown,     T::Hold, M::Walk);
}

void App::initResources() {
    uploadQueue_      = std::make_unique<resource::UploadQueue>(*transientCommandPool_, *rhiFactory_,
                                                                vmaContext_->getAllocator());
    resourceRegistry_ = std::make_unique<resource::ResourceRegistry>(vmaContext_->getAllocator(),
                                                                     *uploadQueue_, *rhiFactory_);
    assetLibrary_     = std::make_unique<resource::AssetLibrary>(*resourceRegistry_);
}

void App::initSamplers() {
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
    albedoSampler_ = std::make_unique<Sampler>(vulkanDevice_.device, albedoInfo);

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
    normalSampler_ = std::make_unique<Sampler>(vulkanDevice_.device, normalInfo);
}

void App::initContent() {
    demoScene_.build(*albedoSampler_, *normalSampler_,
                     *assetLibrary_, *resourceRegistry_, *uploadQueue_);
}

void App::initRender() {
    RenderContext rct = vulkanDevice_.renderContext();

    const auto& spvCode = shaderManager_->spirv(config_.shaderPath);
    descriptorSetLayout_ = std::make_unique<render::DescriptorSetLayout>(rct, spvCode);
    descriptorPool_      = std::make_unique<render::DescriptorPool>(rct,
                                                            descriptorSetLayout_->getPoolMaxSets(),
                                                            descriptorSetLayout_->getPoolSize());

    // One Set-1 descriptor set per material (shared materials reuse the same
    // set no matter how many objects reference it).
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(descriptorPool_->getDescriptorPool())
             .setDescriptorSetCount(1)
             .setSetLayouts(descriptorSetLayout_->getLayoutHandles()[1]);
    for (const auto& material : demoScene_.materials()) {
        material->createDescriptorSet(rct, allocInfo);
    }

    // MSAA: honor Config, clamped to device limits; keep the device-derived
    // contexts in agreement with what the pipeline/attachments will use.
    const vk::SampleCountFlagBits chosenMsaa =
        pickMsaaCount(vulkanDevice_.physicalDevice, config_.msaaSamples);
    vulkanDevice_.msaaSamples = chosenMsaa;
    const render::RenderSettings settings{chosenMsaa, config_.preferredPresentMode};

    RenderContext rctForRenderer = vulkanDevice_.renderContext();
    render::Renderer::Dependencies deps{
        .rct          = rctForRenderer,
        .alloc        = vmaContext_->getAllocator(),
        .setLayouts   = descriptorSetLayout_->getLayoutHandles(),
        .set0Pool     = *descriptorPool_->getDescriptorPool(),
        .graphicsPool = *graphicsCommandPool_,
        .cameras      = cameraManager_,
        .frameParams  = demoScene_.frameParams(),
        .surface      = surface_->handle(),
        .window       = *window_,
        .spirvPath    = config_.shaderPath,
        .factory      = *rhiFactory_,
    };
    renderer_ = std::make_unique<render::Renderer>(deps, settings);
}

}  // namespace app
