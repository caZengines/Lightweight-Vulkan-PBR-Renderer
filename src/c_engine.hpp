#pragma once
#include "app/config.hpp"
#include "camera.hpp"
#include "generic/material.hpp"
#include "generic/scene.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "render/descriptor_manager.hpp"
#include "render/renderer.hpp"
#include "render/shader_manager.hpp"
#include "resource/asset_library.hpp"
#include "resource/resource_registry.hpp"
#include "resource/upload_queue.hpp"
#include "rhi/debug_messenger.hpp"
#include "rhi/rhi_factory.hpp"
#include "rhi/surface.hpp"
#include "rhi/vma_allocator.hpp"
#include "rhi/vulkan_device.hpp"
#include <memory>
#include <vector>

inline const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

inline std::vector<const char *> requiredDeviceExtensions = {vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

// C++ destroys members in reverse declaration order.
class CEngine final {
    public:
        explicit CEngine();
        ~CEngine(){
            cleanup();
        };

        void run();
    private:

        app::Config                                  config_{};

        std::unique_ptr<platform::Window>            window             = nullptr;
        platform::Input                              input{};
        Camera                                       camera{};
        rhi::VulkanDevice                            vulkanDevice_{};
        std::unique_ptr<rhi::VmaContext>                  vmaContext_        = nullptr;
        // Phase 3 cleanup: the former god-config Context split in two.
        std::unique_ptr<rhi::DebugMessenger>         debugMessenger     = nullptr;
        std::unique_ptr<rhi::Surface>                surface            = nullptr;

        // Phase 3: explicit (non-singleton) RHI helper + shared shader cache.
        std::unique_ptr<rhi::RhiFactory>             rhiFactory_        = nullptr;
        std::unique_ptr<render::ShaderManager>       shaderManager_     = nullptr;

        std::unique_ptr<CommandPool>                 graphicsCommandPool  = nullptr;
        std::unique_ptr<CommandPool>                 transientCommandPool = nullptr;

        std::unique_ptr<resource::UploadQueue>       uploadQueue_        = nullptr;
        std::unique_ptr<resource::ResourceRegistry>  resourceRegistry_   = nullptr;
        std::unique_ptr<resource::AssetLibrary>      assetLibrary_       = nullptr;

        std::unique_ptr<Sampler>                     albedoSampler        = nullptr;
        std::unique_ptr<Sampler>                     normalSampler        = nullptr;

        std::unique_ptr<render::DescriptorSetLayout> descriptorSetLayout  = nullptr;
        // Must be declared before any member that holds vk::raii::DescriptorSet
        // (Materials, per-frame sets inside Renderer) so the pool outlives its
        // allocated sets.
        std::unique_ptr<render::DescriptorPool>      descriptorPool       = nullptr;

        std::shared_ptr<Material>                    defaultMaterial      = nullptr;
        std::shared_ptr<Material>                    MarsMaterial         = nullptr;
        std::shared_ptr<Material>                    rockMaterial         = nullptr;

        Scene                                        scene_{};

        std::unique_ptr<render::Renderer>            renderer             = nullptr;

        void initWindow();
        void initVulkan();
        void cleanup();

        // Keyboard / scroll camera input — polled from platform::Input each frame
        void updateCamera(float deltaTime);

        void createCommandPools();
        void initAssetLibrary();
        void createSamplers();
        void createMaterials();
        void createDescriptorSetLayout();
        void createDescriptorSetPool();
        void initScene();
        void initRenderer();
};
