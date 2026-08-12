#include "camera.hpp"
#include "vma_allocator.hpp"
#include "generic/scene.hpp"
#include "vulkandevice.hpp"
#include "context.hpp"
#include "asset_manager.hpp"
#include "renderer/renderer.hpp"
#include <memory>

inline constexpr uint32_t WIDTH = 1920;
inline constexpr uint32_t HEIGHT = 1080;

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

        GLFWwindow*                              window  = nullptr;
        Camera                                   camera{};
        VulkanDevice                             vulkanDevice_{};
        vk::raii::Context                        ct;
        std::unique_ptr<VmaContext>              vmaContext_          = nullptr;
        std::unique_ptr<Context>                 context              = nullptr;

        std::unique_ptr<CommandPool>             graphicsCommandPool  = nullptr;
        std::unique_ptr<CommandPool>             transientCommandPool = nullptr;

        AssetManager                             assetManage {};
        std::unique_ptr<Sampler>                 albedoSampler        = nullptr;
        std::unique_ptr<Sampler>                 normalSampler        = nullptr;

        std::unique_ptr<DescriptorSetLayout>     descriptorSetLayout  = nullptr;
        // Must be declared before any member that holds vk::raii::DescriptorSet
        // (Materials, PerFrameDescriptorSet via Renderer) so the pool outlives
        // its allocated sets.
        std::unique_ptr<DescriptorPool>          descriptorPool       = nullptr;

        std::shared_ptr<Material>                MarsMaterial         = nullptr;
        std::shared_ptr<Material>                rockMaterial         = nullptr;
        std::vector<InstanceData>                instanceDatas;

        // Pre-created 1×1 fallback textures – safe to use when a real texture is unavailable.
        std::shared_ptr<Texture>                 defaultAlbedoTexture_  = nullptr;
        std::shared_ptr<Texture>                 defaultNormalTexture_  = nullptr;

        Scene                                    scene_{};

        std::unique_ptr<Renderer>                renderer             = nullptr;
        
        uint32_t                                 mipLevels;
 

        void initWindow();
        void initVulkan();
        void cleanup();

        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/);
        static void cursorPosCallback(GLFWwindow* window, double xPos, double yPos);
        static void glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height);
        static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

        void createCommandPools();
        void initAssetManager();
        void loadTextures();
        void createSamplers();
        void createMaterials();
        void createDescriptorSetLayout();
        void createDescriptorSetPool();
        void initScene();
        void initRenderer();
};