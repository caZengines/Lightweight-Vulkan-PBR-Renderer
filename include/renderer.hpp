#pragma once
#include "command_manager.hpp"
#include "descriptor_manager.hpp"
#include "generic/scene.hpp"
#include "swapchain.hpp"
#include "render_context.hpp"
#include "pipeline_layout.hpp"
#include "camera.hpp"
#include "vulkan/vulkan.hpp"
#include <vector>
#include <memory>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

inline int instanceText = 1;

struct Light {
  alignas(16) glm::vec4 pos;
  alignas(16) glm::vec4 color;
  alignas(16) float intensity;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;

    alignas(16) glm::vec4 camPos;
    alignas(16) Light     light;
};

class Renderer final {
    public:
    bool     framebufferResized = false;

    explicit Renderer(RenderContext& rct,
                      const std::vector<vk::DescriptorSetLayout>& dsls,
                      const vk::DescriptorPool& pool,
                      Camera& camera,
                      CommandPool& commandPool,
                      vk::raii::SurfaceKHR& surface,
                      GLFWwindow* window);
    ~Renderer(){
        if(!cleaned_) cleanup();
    }

    void drawFrame(const std::vector<DrawBatch>& batches);
    void cleanup();

    private:
        GLFWwindow*                              window_;
        vk::raii::SurfaceKHR&                    surface_;
        RenderContext                            rct_;
        std::vector<vk::DescriptorSetLayout>     descriptorSetLayouts_;
        vk::DescriptorPool                       descriptorPool_;
        Camera&                                  camera_;

        std::unique_ptr<Swapchain>               swapchainInfo          = nullptr;
        std::unique_ptr<Pipeline>                graphicsPipeline       = nullptr;
        std::unique_ptr<PerFrameDescriptorSet>   perframeDescriptorSet_ = nullptr;

        std::vector<vk::raii::DeviceMemory>      uniformBuffersMemory;
        std::vector<vk::raii::Buffer>            uniformBuffers;
        std::vector<void *>                      uniformBuffersMapped;

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
        void createCommandBuffers(CommandPool& commandPool);
        void createSyncObjects();
        void recreateAfterResize();
        void destroySyncObjects();

        void updateUniformBuffer(uint32_t);
        void updateDescriptorSet(uint32_t);
        void recordCommandBuffer(uint32_t ImageIndex, const std::vector<DrawBatch>& batches);
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