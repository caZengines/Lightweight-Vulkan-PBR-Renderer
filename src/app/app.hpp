#pragma once
#include "app/camera_controller.hpp"
#include "app/config.hpp"
#include "app/demo_scene.hpp"
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
#include "scene/camera.hpp"
#include "scene/scene.hpp"
#include <memory>

namespace rhi {
class CommandPool;
}

namespace app {

// owns every subsystem and assembles them
// bottom-up (rhi → resource → render services → content). run() only wires
// the loop and forwards — all assembly happens in the constructor.
//
// Destruction discipline (C++ destroys members in reverse declaration order):
// every member that transitively holds GPU resources — VMA allocations
// (scene_ → InstanceBuffer → VmaBuffer) or vk::raii handles (materials'
// descriptor sets, Renderer's per-frame sets) — must stay declared AFTER the
// rhi/resource members it depends on (vulkanDevice_/vmaContext_,
// descriptorPool_). Declaring a GPU-resource holder above them means the
// allocator/pool dies first and VMA asserts with
// "Some allocations were not freed before destruction" at exit.
class App final {
public:
    App();
    ~App();

    void run();

private:
    // Assembly steps, bottom-up.
    void initWindow();
    void initRhi();         // device, VMA, factories/messengers, command pools
    void initResources();   // upload queue, registry, asset library
    void initSamplers();
    void initContent();     // DemoScene: materials + scene objects
    void initRender();      // descriptor layout/pool, material sets, renderer

    // --- configuration, window & input (Layer 1) ---
    Config                            config_{};
    std::unique_ptr<platform::Window> window_;
    platform::Input                   input_{};

    // --- camera: scene-layer math + app-side input mapping ---
    scene::Camera    camera_{};
    CameraController cameraController_{camera_};

    // --- rhi (Layer 0) ---
    rhi::VulkanDevice                           vulkanDevice_{};
    std::unique_ptr<rhi::VmaContext>            vmaContext_;
    std::unique_ptr<rhi::DebugMessenger>        debugMessenger_;
    std::unique_ptr<rhi::Surface>               surface_;
    std::unique_ptr<rhi::RhiFactory>            rhiFactory_;
    std::unique_ptr<render::ShaderManager>      shaderManager_;
    std::unique_ptr<rhi::CommandPool>           graphicsCommandPool_;
    std::unique_ptr<rhi::CommandPool>           transientCommandPool_;

    // --- resource (Layer 2) ---
    std::unique_ptr<resource::UploadQueue>       uploadQueue_;
    std::unique_ptr<resource::ResourceRegistry>  resourceRegistry_;
    std::unique_ptr<resource::AssetLibrary>      assetLibrary_;
    std::unique_ptr<Sampler>                     albedoSampler_;
    std::unique_ptr<Sampler>                     normalSampler_;

    // --- render services (Layer 3) ---
    std::unique_ptr<render::DescriptorSetLayout> descriptorSetLayout_;
    std::unique_ptr<render::DescriptorPool>      descriptorPool_;

    // --- scene data (filled by DemoScene) ---
    scene::Scene                                 scene_{};

    // --- content & frame orchestration (after the pool, per the rule above) ---
    DemoScene                                    demoScene_{config_, scene_};
    std::unique_ptr<render::Renderer>            renderer_;
};

}  // namespace app
