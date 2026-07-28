#include "c_engine.hpp"
#include "context.hpp"
#include "descriptor_manager.hpp"
#include "generic/renderobject.hpp"
#include "generic/vertex.hpp"
#include "render_context.hpp"
#include <cstddef>
#include <random>
#include <memory>

CEngine::CEngine(){
    initWindow();
    initVulkan();
}

void CEngine::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "C' Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, glfwFramebufferResizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallBack);
    glfwSetCursorPosCallback(window, cursorPosCallBack);
    glfwSetScrollCallback(window, scrollCallBack);
}
void CEngine::glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app                = static_cast<CEngine*>(glfwGetWindowUserPointer(window));
    app->renderer->framebufferResized = true;
}

void CEngine::initVulkan() {
    VulkanDevice::CreateInfo deviceInfo;
    deviceInfo.requiredDeviceExtensions_ = requiredDeviceExtensions;
    deviceInfo.appName = "No Engine";
    deviceInfo.enableValidationLayers_= enableValidationLayers;
    deviceInfo.window_ = window;
    vulkanDevice_.init(deviceInfo);
    ResourceFactory::init(vulkanDevice_.physicalDevice, vulkanDevice_.device);
    Context::Config cfg;
    cfg.window_                 = window;
    cfg.enableValidationLayers_ = enableValidationLayers;
    cfg.validationLayers_       = validationLayers;
    cfg.msaaSamples_            = vulkanDevice_.msaaSamples;
    context = std::make_unique<Context>(cfg, vulkanDevice_.physicalDevice, vulkanDevice_.device, vulkanDevice_.instance);
    createCommandPools();
    initAssetManager();
    createSamplers();
    createMaterials();
    createDescriptorSetLayout();
    createDescriptorSetPool();
    initScene();
    initRenderer();
}

void CEngine::run() {
    const auto& batches = scene_.getDrawBatches();
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        renderer->drawFrame(batches);
    }
    vulkanDevice_.device.waitIdle();
}

void CEngine::cleanup() {
    renderer->cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void CEngine::createMaterials() {
    auto albedo = assetManage.findTexture(MARS_PATH);
    if (!albedo) albedo = defaultAlbedoTexture_;
    MarsMaterial = std::make_shared<Material>(albedo, defaultNormalTexture_,
                                              *albedoSampler,
                                              *normalSampler);
    rockMaterial = std::make_shared<Material>(assetManage.findTexture(ROCK_TEXTURE_PATH),defaultNormalTexture_,
                                              *albedoSampler,
                                              *normalSampler);
}

void CEngine::createDescriptorSetLayout() {
    auto spvCode = Pipeline::readFile("../shaders/slang.spv");
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
         descriptorSetLayout->getLayoutHandles(),
         *descriptorPool->getDescriptorPool(),
         camera,
         *graphicsCommandPool,
         context->surface,
         window);
 }

void CEngine::createCommandPools() {
    graphicsCommandPool  = std::make_unique<CommandPool>(vulkanDevice_.device, vulkanDevice_.graphicsQueueIndex, std::move(vulkanDevice_.graphicsQueue), vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    transientCommandPool = std::make_unique<CommandPool>(vulkanDevice_.device, vulkanDevice_.transferQueueIndex, std::move(vulkanDevice_.transferQueue),
                                              vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                                                        | vk::CommandPoolCreateFlagBits::eTransient);
}

void CEngine::initAssetManager() {
    //assetManage.loadTexture(TEXTURE_PATH, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear, *graphicsCommandPool);
    //assetManage.loadTexture(NORMAL_PATH, vk::Format::eR8G8B8A8Unorm, vk::Filter::eNearest, *graphicsCommandPool);
    assetManage.loadTexture(ROCK_TEXTURE_PATH, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear, *graphicsCommandPool);
    assetManage.loadTexture(MARS_PATH, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear, *graphicsCommandPool);

    // --- Pre-create 1×1 fallback textures (always available, no file dependency) ---
    defaultAlbedoTexture_ = std::make_shared<Texture>(
        Texture::createDefaultAlbedo(*graphicsCommandPool));
    defaultNormalTexture_ = std::make_shared<Texture>(
        Texture::createDefaultNormal(*graphicsCommandPool));

    assetManage.loadMesh(ROCK_PATH, *transientCommandPool);
    assetManage.loadMesh(PLANET_PATH, *transientCommandPool);
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
    std::shared_ptr<RenderObject> mars = std::make_shared<RenderObject>(assetManage.getMesh(PLANET_PATH), MarsMaterial);
    std::vector<InstanceData> instances(1);
    glm::mat4 marsmodel = glm::mat4(1.0f);
    marsmodel = glm::translate(marsmodel, glm::vec3(0.0f, -3.0f, 0.0f));
    marsmodel = glm::scale(marsmodel, glm::vec3(2.0f, 2.0f, 2.0f));
    instances[0].model = marsmodel;
    mars->setInstances(instances, *transientCommandPool);
    mars->initMaterialDescriptor(RCT, descriptorSetLayout->getLayoutHandles()[1], *descriptorPool);
    scene_.addObject(std::move(mars));

    std::shared_ptr<RenderObject> rock = std::make_shared<RenderObject>(assetManage.getMesh(ROCK_PATH), rockMaterial);
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
    rock->setInstances(rocks, *transientCommandPool);
    rock->initMaterialDescriptor(RCT, descriptorSetLayout->getLayoutHandles()[1], *descriptorPool);
    scene_.addObject(std::move(rock));
}