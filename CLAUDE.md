# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A lightweight Vulkan-based PBR renderer in early framework construction. C++20, Vulkan SDK 1.4.341.1, uses Vulkan-Hpp (C++ RAII bindings). Final target: path tracing.

**Key dependencies:** Vulkan SDK, GLFW, GLM, [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect), [Slang](https://github.com/shader-slang/slang) (shader compiler).

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The CMake build automatically compiles `shaders/shader.slang` to `shaders/slang.spv` via `slangc` before linking. The executable is `bin/main` (or `bin/main.exe` on Windows). Two configs: `Debug` (`-g -O0`) and `Release` (`-O3 -march=native -DNDEBUG`).

Validation layers are enabled in Debug, disabled in Release (`#ifdef NDEBUG`).

Build defines `SPIRV_REFLECT_USE_SYSTEM_SPIRV_H` — SPIRV-Reflect expects the SPIRV headers to be on the include path (provided by the Vulkan SDK).

There are no tests or linting configured yet.

## Architecture

### Initialization flow (CEngine)

`src/main/main.cpp` creates a `CEngine` which initializes in order:

1. **initWindow()** — GLFW window, no-API, resizable. Callbacks are static methods that cast `glfwGetWindowUserPointer` back to `CEngine*`.
2. **initVulkan()**:
   - `VulkanDevice::init()` — creates instance, picks physical device, creates logical device, sets up graphics+transfer queue indices and handles.
   - `VmaContext` — wraps the VMA allocator.
   - `ResourceFactory::init()` — singleton holding physical device & device pointers for on-demand image-view / layout-transition helpers.
   - `Context` — debug messenger + window surface.
   - Two `CommandPool`s: `graphicsCommandPool` (reset per frame) and `transientCommandPool` (transient, used for staging uploads).
   - `AssetManager::loadTexture` / `loadMesh` — loads and caches assets.
   - `DescriptorSetLayout` — reflects SPIR-V to auto-generate descriptor set layouts.
   - `DescriptorPool` — allocates from those layouts.
   - Scene setup: creates `RenderObject`s, sets instanced transforms.
   - `Renderer` — swapchain, graphics pipeline, per-frame UBOs, sync primitives, command buffers.

### Core classes

| Class | Role |
|---|---|
| `VulkanDevice` | Instance, physical device, logical device, queue indices/handles. `renderContext()` packs these into a `RenderContext` struct. |
| `VmaContext` | Owns the `VmaAllocator`. `VmaBuffer` / `VmaImage` are RAII move-only wrappers around VMA allocations. |
| `CommandPool` | Wraps `vk::raii::CommandPool` + `vk::raii::Queue`. Has `beginSingleTimeCommands()` / `endSingleTimeCommands()` for one-shot submit. |
| `ResourceFactory` | Singleton. Creates image views, does `copyBufferToImage`, `transitionImageLayout`, format queries. |
| `Renderer` | The core rendering loop. Owns swapchain, graphics pipeline, per-frame UBOs, sync objects (semaphores, fences). `drawFrame()` acquires swapchain image, updates UBO+descriptor, records command buffer, submits, presents. Handles resize via `recreateAfterResize()`. |
| `Pipeline` | Wraps `vk::raii::Pipeline` + `PipelineLayout`. Reads SPIR-V, creates shader modules, builds graphics pipeline with depth testing. |
| `DescriptorSetLayout` | Takes SPIR-V bytecode, uses SPIRV-Reflect to auto-build `vk::DescriptorSetLayout` objects. Stores `ReflectBinding` metadata and computes pool sizes. |
| `DescriptorPool` / `DescriptorSet` / `PerFrameDescriptorSet` | RAII wrappers for descriptor pool allocation. `PerFrameDescriptorSet` creates `MAX_FRAMES_IN_FLIGHT` copies and updates the UBO binding. |

### Scene graph

- `Scene` holds `vector<shared_ptr<RenderObject>>`.
- `Scene::getDrawBatches()` flattens objects into `DrawBatch`es, merging instances that share the same `(Material, Mesh)` pair. The result is cached until `batchesDirty_` is set.
- `RenderObject` binds a `Mesh`, `Material`, and instance data (via `Buffer<InstanceData>`).
- `Mesh` loads an OBJ, deduplicates vertices, creates vertex/index GPU buffers.
- `Material` holds albedo + normal textures + samplers, creates a per-material descriptor set, stores `RenderFlags` (bitmask for which textures are available).

### Data flow per frame

1. `CEngine::run()` calls `scene_.getDrawBatches()` once (cached), then loops `renderer->drawFrame(batches)`.
2. `drawFrame`: acquire swapchain image → update UBO (view/proj/camera pos/light) → update per-frame descriptor → record command buffer (bind pipeline, bind Set 0=UBO and Set 1=per-material textures, push constants for flags, instanced draw) → submit → present.

### Descriptor set layout (binding model)

SPIR-V reflection produces two sets:
- **Set 0** (per-frame): UBO at binding 0 (`Binding::kUbo`).
- **Set 1** (per-material): combined image sampler at binding 0 (`Binding::kAlbedoTexture`), combined image sampler at binding 1 (`Binding::kNormalTexure`).

Push constants carry `RenderFlags` (bit 0 = albedo texture, bit 1 = normal map).

### Memory management

All GPU resources use VMA. `Buffer<T>` is a templated GPU buffer that stages data through a HOST_VISIBLE staging buffer then copies to a DEVICE_LOCAL buffer. `Texture` creates images with mipmaps via `ResourceFactory`. Assets are cached in `AssetManager` by file path string key.

### Important notes

- **AGENTS.md is stale** — it documents the VMA library API itself, not this project. This project *uses* VMA but is not the VMA library.
- The Slang shader has two entry points: `vertMain` and `fragMain`, output as SPIR-V 1.4.
- `Renderer::transition_image_layout` takes `VkImage` (C handle), not `vk::Image` — intentional for interop with VMA-allocated images.
- GLM is configured with `GLM_FORCE_DEPTH_ZERO_TO_ONE` (Vulkan clip space).
- Move-only semantics throughout — `VmaBuffer`, `VmaImage`, `Texture`, `Material`, `RenderObject` all ban copies.
