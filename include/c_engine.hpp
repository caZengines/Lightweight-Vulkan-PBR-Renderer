#include "camera.hpp"
#include "context.hpp"
#include "generic/texture.hpp"
#include "generic/model.hpp"
#include "material.hpp"
#include "renderer.hpp"
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

class CEngine {
    public:
        explicit CEngine();
        ~CEngine(){
            cleanup();
        };

        void run();
    private:

        GLFWwindow*                              window  = nullptr;
        Camera                                   camera;
        vk::raii::Instance                       instance             = nullptr;
        vk::raii::PhysicalDevice                 physicalDevice       = nullptr;
        vk::raii::Device                         device               = nullptr;
        vk::raii::Context                        ct;
        std::unique_ptr<Context>                 context              = nullptr;

        vk::raii::Queue                          graphicsQueue        = nullptr;
        vk::raii::Queue                          transferQueue        = nullptr;
        uint32_t                                 graphicsQueueIndex   = ~0;
        uint32_t                                 transferQueueIndex   = ~0;

        std::unique_ptr<CommandPool>             graphicsCommandPool  = nullptr;
        std::unique_ptr<CommandPool>             transientCommandPool = nullptr;


        vk::SampleCountFlagBits                  msaaSamples          = vk::SampleCountFlagBits::e1;

        std::unique_ptr<Texture>                 albedoTexture        = nullptr;
        std::unique_ptr<Texture>                 NormalTexture        = nullptr;

        std::unique_ptr<obj_Model>               mainModel            = nullptr;
        std::unique_ptr<Material>                mainMaterial         = nullptr;

        std::unique_ptr<Renderer>                renderer             = nullptr;
        
        uint32_t                                 mipLevels;
 

        void initWindow();
        void initVulkan();
        void cleanup();

        static void mouseButtonCallBack(GLFWwindow* window, int button, int action, int /*mods*/);
        static void cursorPosCallBack(GLFWwindow* window, double xPos, double yPos);
        static void glfwFramebufferResizeCallback(GLFWwindow* window, int width, int height);

        void createInstance();
        void pickPhysicalDevice();
        void createLogicalDevice();
        bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice);
        void setSampleCount();
        std::vector<const char*> GetRequiredExtension();

        void createSurface();
        void createCommandPools();
        void createTextures();
        void loadModel();
        void initRenderer();
};