#include "rhi/vulkan_device.hpp"
#include "platform/log.hpp"
#include <map>
#include <string>

namespace rhi {

void VulkanDevice::init(const CreateInfo& info){
    info_ = info;
    createInstance();
    pickPhysicalDevice();
    //checkFeatureSupport();
    createLogicalDevice();
}

void VulkanDevice::createInstance(){
    vk::ApplicationInfo appIF;
    appIF.setPApplicationName("Render Engine")
           .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
           .setPEngineName("No Engine")
           .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
           .setApiVersion(vk::ApiVersion13);
    //get required layers
    std::vector<const char*> RequiredLayers;
    if(info_.enableValidationLayers_){
        RequiredLayers.assign(info_.validationLayers_.begin(), info_.validationLayers_.end());
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
    createInfo.setPApplicationInfo(&appIF)
                      .setEnabledLayerCount(static_cast<uint32_t>(RequiredLayers.size()))
                      .setPpEnabledLayerNames(RequiredLayers.data())
                      .setEnabledExtensionCount(static_cast<uint32_t>(RequiredExtensions.size()))
                      .setPpEnabledExtensionNames(RequiredExtensions.data());

    instance = vk::raii::Instance(context_, createInfo);
}
std::vector<const char*> VulkanDevice::GetRequiredExtension(){
    // Instance extensions come from the platform layer (window system),
    // not from GLFW directly.
    std::vector extensions = info_.instanceExtensions_;
    if(info_.enableValidationLayers_){
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

void VulkanDevice::pickPhysicalDevice(){
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
        if(info_.enableValidationLayers_){
            vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
            platform::LogLocator::get().write(platform::LogLevel::Info,
                std::string("GPU Information: ") + deviceProperties.deviceName.data());
            std::string apiVersion = std::to_string(VK_VERSION_MAJOR(deviceProperties.apiVersion)) + "." +
                                     std::to_string(VK_VERSION_MINOR(deviceProperties.apiVersion)) + "." +
                                     std::to_string(VK_VERSION_PATCH(deviceProperties.apiVersion));
            platform::LogLocator::get().write(platform::LogLevel::Info, "API Version: " + apiVersion);
        }
        setSampleCount();
    }
    else{
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}
bool VulkanDevice::isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice){
    // Check if the physicalDevice supports the Vulkan 1.3 API version
    bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

    // Check if any of the queue families support graphics operations
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp)
                                                    { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

    // Check if all required physicalDevice extensions are available
    auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions =
        std::ranges::all_of(info_.requiredDeviceExtensions_,
                            [&availableDeviceExtensions](auto const &requiredDeviceExtension)
                            {
                                return std::ranges::any_of(availableDeviceExtensions,
                                                           [requiredDeviceExtension](auto const &availableDeviceExtension)
                                                           { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                            });

    // Return true if the physicalDevice meets all the criteria
    return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions;
}

void VulkanDevice::createLogicalDevice(){
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

    //Create graphic & transfer queue
    std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo graphicQueueCreateInfo{};
    graphicQueueCreateInfo.setQueueFamilyIndex(graphicsQueueIndex)
                         .setQueueCount(1)
                         .setPQueuePriorities(&queuePriority);
        queueCreateInfos.emplace_back(graphicQueueCreateInfo);
    vk::DeviceQueueCreateInfo transferQueueCreateInfo{};
    transferQueueCreateInfo.setQueueFamilyIndex(transferQueueIndex)
                           .setQueueCount(1)
                           .setPQueuePriorities(&queuePriority);
    queueCreateInfos.emplace_back(transferQueueCreateInfo);
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

    vk::DeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.setPNext(&featureChain.get<vk::PhysicalDeviceFeatures2>())
                    .setQueueCreateInfos(queueCreateInfos)
                    .setEnabledExtensionCount(static_cast<uint32_t>(requiredDeviceExtension.size()))
                    .setPpEnabledExtensionNames(requiredDeviceExtension.data());
    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsQueueIndex, 0);

    transferQueue = vk::raii::Queue(device, transferQueueIndex, 0);
}

// void VulkanDevice::checkFeatureSupport(){  *This function will be used in future.*
//     // Define the KHR roadmap 2022 profile
//     appInfo.profile = {
//         VP_KHR_ROADMAP_2022_NAME,
//         VP_KHR_ROADMAP_2022_SPEC_VERSION
//     };
//     VkBool32 supported = false;
//     VkResult result = vpGetPhysicalDeviceProfileSupport(*instance, *physicalDevice, &appInfo.profile, &supported);
//     if (result == VK_SUCCESS && supported == VK_TRUE){
// 		appInfo.profileSupported = true;
// 		std::cout << "Using KHR roadmap 2022 profile" << std::endl;
// 	}
//     else {
//         appInfo.profileSupported = false;
// 		std::cout << "Falling back to traditional rendering (profile not supported)" << std::endl;
//     }
// }

void VulkanDevice::setSampleCount(){
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
    std::string msaaCounts;
    for (const auto& [count, flag] : usableSampleCounts) {
        msaaCounts += std::to_string(count) + " ";
    }
    platform::LogLocator::get().write(platform::LogLevel::Info, "Select MSAA Sample count: " + msaaCounts);
    msaaSamples = vk::SampleCountFlagBits::e4;
    platform::LogLocator::get().write(platform::LogLevel::Info, "Anti-Aliasing Mode: MSAA X4");
    // std::cout << '\n';
    // while (true) {
    //     int idx = 0;
    //     if (!(std::cin >> idx)){
    //         std::cin.clear();
    //         std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    //         std::cerr << "Invalid input, please enter a number.\n";
    //         continue;
    //     }

    //     if (auto it = usableSampleCounts.find(idx); it != usableSampleCounts.end()) {
    //         msaaSamples = it->second;
    //         std::cout << "Anti-Aliasing Mode: MSAA X" << idx << '\n';
    //         break;
    //     } else {
    //         std::cerr << "Unsupported sample count! \n";
    //     }
    // }
}
}  // namespace rhi
