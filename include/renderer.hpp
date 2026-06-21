#pragma once
#include "command_manager.hpp"
#include "descriptor_manager.hpp"
#include "swapchain.hpp"
#include "render_context.hpp"
#include "pipeline_layout.hpp"
#include "material.hpp"
#include "generic/model.hpp"
#include "camera.hpp"
#include <vector>
#include <memory>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct Light {
  alignas(16) glm::vec4 pos;
  alignas(16) glm::vec4 color;
  alignas(16) float intensity;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;

    alignas(16) glm::vec4 camPos;
    alignas(16) Light     light;
};

class Renderer{
    public:
    bool     framebufferResized = false;

    explicit Renderer(RenderContext& rct, vk::raii::DescriptorSetLayout& dsl, Material& material, obj_Model& model, Camera& camera, CommandPool& commandPool, vk::raii::SurfaceKHR& surface, GLFWwindow* window);
    ~Renderer(){
        if(!cleaned_) cleanup();
    }

    void drawFrame();
    void cleanup();

    private:
        GLFWwindow*                              window_              = nullptr;
        vk::raii::SurfaceKHR&                    surface_;
        RenderContext                            rct_;
        vk::raii::DescriptorSetLayout&           dsl_;
        Material&                                material_;
        obj_Model&                               model_;
        Camera&                                  camera_;
        std::unique_ptr<Swapchain>               swapchainInfo        = nullptr;
        std::unique_ptr<Pipeline>                graphicsPipeline     = nullptr;

        std::vector<vk::raii::DeviceMemory>      uniformBuffersMemory;
        std::vector<vk::raii::Buffer>            uniformBuffers;
        std::vector<void *>                      uniformBuffersMapped;

        std::unique_ptr<DescriptorPool>          descriptorPool       = nullptr;
        std::vector<vk::raii::DescriptorSet>     descriptorSets;

        std::vector<vk::raii::CommandBuffer>     graphicsCommandBuffers;

        CommandPool&                             graphicsCommandPool;

        std::vector<vk::raii::Semaphore>         presentCompleteSemaphores;
        std::vector<vk::raii::Semaphore>         presentWaitSemaphores;
        vk::raii::Semaphore                      renderFinishedTimelineSemaphore = nullptr;
        std::vector<vk::raii::Fence>             inFlightFences;

        uint32_t                                 frameIndex       = 0;

        uint64_t                                 frameCount       = 0;

        bool                                     cleaned_         = false;

        void createGraphicsPipeline();
        void createUniformBuffers();
        void createDescriptorPoolAndSets();
        void createCommandBuffers(CommandPool& commandPool);
        void createSyncObjects();
        void recreateAfterResize();
        void destroySyncObjects();

        void updateUniformBuffer(uint32_t frameIndex);
        void recordCommandBuffer(uint32_t ImageIndex);
        void transition_image_layout(
                                 vk::Image               image,
                                 vk::ImageLayout         old_layout,
                                 vk::ImageLayout         new_layout,
                                 vk::AccessFlags2        src_access_mask,
                                 vk::AccessFlags2        dst_access_mask,
                                 vk::PipelineStageFlags2 src_stage_mask,
                                 vk::PipelineStageFlags2 dst_stage_mask,
                                 vk::ImageAspectFlags    image_aspect_flags
        );
};