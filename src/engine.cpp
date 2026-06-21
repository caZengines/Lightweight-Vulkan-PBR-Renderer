#include "c_engine.hpp"
#include "context.hpp"
#include "material.hpp"
#include "render_context.hpp"
#include "renderer.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <map>
#include <memory>

CEngine::CEngine(){
    initWindow();
    initVulkan();
}

void CEngine::initWindow(){
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "C' Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, glfwFramebufferResizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallBack);
    glfwSetCursorPosCallback(window, cursorPosCallBack);
}
void CEngine::glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height){
    auto app                = static_cast<CEngine*>(glfwGetWindowUserPointer(window));
    app->renderer->framebufferResized = true;
}

void CEngine::initVulkan(){
    createInstance();
    pickPhysicalDevice();
    createLogicalDevice();
    ResourceFactory::init(physicalDevice, device);
    Context::Config cfg;
    cfg.window_                 = window;
    cfg.enableValidationLayers_ = enableValidationLayers;
    cfg.validationLayers_       = validationLayers;
    cfg.msaaSamples_            = msaaSamples;
    context = std::make_unique<Context>(cfg, physicalDevice, device, instance, std::move(ct));
    createCommandPools();
    createTextures();
    loadModel();
    initRenderer();
}

void CEngine::run(){
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        renderer->drawFrame();
    }
    device.waitIdle();
}

void CEngine::cleanup(){
    renderer->cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void CEngine::createTextures(){
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
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
    albedoTexture = std::make_unique<Texture>(Texture::createTexture(TEXTURE_PATH.c_str(), vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear, albedoInfo, *graphicsCommandPool));

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
    NormalTexture = std::make_unique<Texture>(Texture::createTexture(NORMAL_PATH.c_str(), vk::Format::eR8G8B8A8Unorm, vk::Filter::eNearest, normalInfo, *graphicsCommandPool));
}

void CEngine::loadModel(){
    mainModel = std::make_unique<obj_Model>(MODEL_PATH, *transientCommandPool);
    std::cout <<"Number of vertices: " <<mainModel->getVertices().size() <<"\n";
}

void CEngine::initRenderer(){
    mainMaterial = std::make_unique<Material>(*albedoTexture, *NormalTexture);
    RenderContext RCT = context->renderContext();
    renderer = std::make_unique<Renderer>(RCT,
        context->descriptorSetLayout->getDescriptorSetLayout(),
        *mainMaterial,
        *mainModel,
        camera,
        *graphicsCommandPool,
        context->surface,
        window);
}

void CEngine::pickPhysicalDevice(){
    std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    std::multimap<int, vk::raii::PhysicalDevice> candidates;
    for(auto const &pd : physicalDevices) {
        if(isDeviceSuitable(pd)) {
        int score = 0;
        auto properties = pd.getProperties();
        if(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;
        else if(properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) score += 500;

        score += properties.limits.maxImageDimension2D;

        candidates.insert(std::make_pair(score, pd));
        }
    }
    if(!candidates.empty() && candidates.rbegin()-> first > 0) {
        physicalDevice = candidates.rbegin()-> second;
        if(enableValidationLayers) std::cout << "GPU Information: " << physicalDevice.getProperties().deviceName << std::endl;
        setSampleCount();
    }
    else{
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool CEngine::isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice){
    // Check if the physicalDevice supports the Vulkan 1.3 API version
    bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

    // Check if any of the queue families support graphics operations
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp)
                                                    { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

    // Check if all required physicalDevice extensions are available
    auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions =
        std::ranges::all_of(requiredDeviceExtensions,
                            [&availableDeviceExtensions](auto const &requiredDeviceExtension)
                            {
                                return std::ranges::any_of(availableDeviceExtensions,
                                                           [requiredDeviceExtension](auto const &availableDeviceExtension)
                                                           { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                            });

    // Check if the physicalDevice supports the required features (dynamic rendering and extended dynamic state)
    auto features =
        physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
                                    features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                    features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
                                    features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

    // Return true if the physicalDevice meets all the criteria
    return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void CEngine::createInstance(){
    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("Hello Triangle")
           .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
           .setPEngineName("No Engine")
           .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
           .setApiVersion(vk::ApiVersion14);
    //get required layers
    std::vector<const char*> RequiredLayers;
    if(enableValidationLayers){
        RequiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }
    auto LayerProperties = vk::enumerateInstanceLayerProperties();
         auto unsupportedLayersIt = std::ranges::find_if(RequiredLayers,
                                                       [&LayerProperties](auto const &RequiredLayer){
                                                            return std::ranges::none_of(LayerProperties, [RequiredLayer](auto const &LayerProperty){
                                                                return strcmp(RequiredLayer, LayerProperty.layerName) == 0;});
                                                        });
    if(unsupportedLayersIt != RequiredLayers.end()){
        throw std::runtime_error(std::string("validation layer requested, but not available: ") + *unsupportedLayersIt);
    }
    //get required extensions
    auto RequiredExtensions = GetRequiredExtension();

    //check if all required extensions are supported
    auto ExtensionProperties = vk::enumerateInstanceExtensionProperties();
        auto unsupportedExtensionsIt = std::ranges::find_if(RequiredExtensions,
                                                       [&ExtensionProperties](auto const &RequiredExtension){
                                                            return std::ranges::none_of(ExtensionProperties, [RequiredExtension](auto const &ExtensionProperty){
                                                                return strcmp(RequiredExtension, ExtensionProperty.extensionName) == 0;});
                                                               });
    if(unsupportedExtensionsIt != RequiredExtensions.end()) {
        throw std::runtime_error(std::string("extension requested, but not available: ") + *unsupportedExtensionsIt);
    }
                                                              
    vk::InstanceCreateInfo createInfo{};
    createInfo.setPApplicationInfo(&appInfo)
                      .setEnabledLayerCount(static_cast<uint32_t>(RequiredLayers.size()))
                      .setPpEnabledLayerNames(RequiredLayers.data())
                      .setEnabledExtensionCount(static_cast<uint32_t>(RequiredExtensions.size()))
                      .setPpEnabledExtensionNames(RequiredExtensions.data());

    instance = vk::raii::Instance(ct, createInfo);
}

std::vector<const char*> CEngine::GetRequiredExtension(){
    uint32_t glfwExtensionsCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
    
    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
    if(enableValidationLayers){
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

void CEngine::createLogicalDevice(){
    // find the index of the first queue family that supports graphics queue
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](const auto &qfp)
                                                            { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
    if(graphicsQueueFamilyProperty != queueFamilyProperties.end()){
        auto graphicIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        graphicsQueueIndex = graphicIndex;
    }
    if(graphicsQueueIndex == ~0) throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    //find the index of the first queue family that supports transfer queue
    auto transferQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [&](const auto& qfp){ 
                                                                if(&qfp == &queueFamilyProperties[graphicsQueueIndex]) return false;
                                                                return (qfp.queueFlags & vk::QueueFlagBits::eTransfer) != static_cast<vk::QueueFlags>(0); 
                                                            });
    if(transferQueueFamilyProperty != queueFamilyProperties.end()){
        auto transferIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), transferQueueFamilyProperty));
        transferQueueIndex = transferIndex;
    }
    if(transferQueueIndex == ~0) throw std::runtime_error("Could not find a queue for transfer and present -> terminating");

    //enabledPhysicalDeviceFeatures
    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                       vk::PhysicalDeviceShaderDrawParametersFeatures,
                       vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR> featureChain;
    auto& deviceFeatures2 = featureChain.get<vk::PhysicalDeviceFeatures2>();
    deviceFeatures2.features.setFillModeNonSolid(true);
    deviceFeatures2.features.setGeometryShader(false);
    deviceFeatures2.features.setSamplerAnisotropy(true);

    featureChain.get<vk::PhysicalDeviceVulkan13Features>().setDynamicRendering(true);
    featureChain.get<vk::PhysicalDeviceVulkan13Features>().setSynchronization2(true);
    featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().setExtendedDynamicState(true);
    featureChain.get<vk::PhysicalDeviceShaderDrawParametersFeatures>().setShaderDrawParameters(true);
    featureChain.get<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>().setTimelineSemaphore(true);
    //Create graphic & transfer queue
    std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo graphicQueueCreateInfo;
    graphicQueueCreateInfo.setQueueFamilyIndex(graphicsQueueIndex)
                         .setQueueCount(1)
                         .setPQueuePriorities(&queuePriority);
        queueCreateInfos.emplace_back(graphicQueueCreateInfo);
    vk::DeviceQueueCreateInfo transferQueueCreateInfo{};
    transferQueueCreateInfo.setQueueFamilyIndex(transferQueueIndex)
                           .setQueueCount(1)
                           .setPQueuePriorities(&queuePriority);
        queueCreateInfos.emplace_back(transferQueueCreateInfo);

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
                    .setQueueCreateInfos(queueCreateInfos)
                    .setEnabledExtensionCount(static_cast<uint32_t>(requiredDeviceExtension.size()))
                    .setPpEnabledExtensionNames(requiredDeviceExtension.data());

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsQueueIndex, 0);

    transferQueue = vk::raii::Queue(device, transferQueueIndex, 0);
}

void CEngine::setSampleCount(){
    vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();
    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    std::map<int, vk::SampleCountFlagBits>     usableSampleCounts;
    usableSampleCounts.insert({1, vk::SampleCountFlagBits::e1});
    constexpr std::array candidates = {
        std::pair{1,  vk::SampleCountFlagBits::e1},
        std::pair{2,  vk::SampleCountFlagBits::e2},
        std::pair{4,  vk::SampleCountFlagBits::e4},
        std::pair{8,  vk::SampleCountFlagBits::e8},
        std::pair{16, vk::SampleCountFlagBits::e16},
        std::pair{32, vk::SampleCountFlagBits::e32},
        std::pair{64, vk::SampleCountFlagBits::e64}
    };
    for (auto [count, flag] : candidates){
        if (counts & flag) {
            usableSampleCounts.emplace(count, flag);
        }
    }
    std::cout << "Select MSAA Sample count: ";
    for (const auto& [count, flag] : usableSampleCounts) {
        std::cout << count << ' ';
    }
    std::cout << '\n';
    while (true) {
        int idx = 0;
        if (!(std::cin >> idx)){
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "Invalid input, please enter a number.\n";
            continue;
        }

        if (auto it = usableSampleCounts.find(idx); it != usableSampleCounts.end()) {
            msaaSamples = it->second;
            std::cout << "Anti-Aliasing Mode: MSAA X" << idx << '\n';
            break;
        } else {
            std::cerr << "Unsupported sample count! \n";
        }
    }
}

void CEngine::createCommandPools(){
    graphicsCommandPool  = std::make_unique<CommandPool>(device, graphicsQueueIndex, std::move(graphicsQueue), vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    transientCommandPool = std::make_unique<CommandPool>(device, transferQueueIndex, std::move(transferQueue),
                                              vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                                                        | vk::CommandPoolCreateFlagBits::eTransient);
}