# 项目进度与当前架构

> 学习型 Vulkan PBR 渲染器 · C++20 · Vulkan-Hpp (RAII) · Vulkan SDK 1.4.341.1
> 最终目标：路径追踪。参考架构：Vulkan 官方教程 *Engine Architecture*（五层分层）+ GPP（Component / Service Locator）。
> 配套文档：`docs/architecture-refactor-plan.md`（Phase 0–6 重构计划）。

---

## 一、进度总览

| 阶段 | 提交 | 状态 | 内容 |
|---|---|---|---|
| Phase 0 基线 | `3cd9eeb` + tag `pre-refactor` | ✅ 完成 | glTF 加载器 WIP、新资源、架构文档、platform 骨架 |
| Phase 1 平台抽象层 | `b50867b` | ✅ 完成 | `platform::Window/Input/Log`（首个 Service Locator），GLFW 收进 `src/platform/` |
| Phase 2 资源管理层 | `e2b9385` | ✅ 完成 | 资源句柄化、导入器、上传队列、注册表；Mesh/Texture 不再自建 buffer |
| Phase 3 渲染层拆分 | — | ⏳ 未开始 | Renderer 只剩帧编排；管线/描述符/录制各归其位 |
| Phase 4 场景层纯数据化 | — | ⏳ 未开始 | RenderItem、SceneObject、Transform 激活、剔除预留 |
| Phase 5 应用层成形 | — | ⏳ 未开始 | `app::App`、GameLoop、CameraController、DemoScene |
| Phase 6 构建层与工程纪律 | — | ⏳ 未开始 | CMake 拆 5 个静态库、命名空间落地、头文件卫生 |

**当前工作树**：干净。**构建环境**：VS Code tasks.json 使用 **Ninja** 生成器（`cmake-build-debug` / `cmake-build-release`），Debug + Release 双配置通过。

### 已完成的验证（Phase 1/2 证据）

- Debug + Release（Ninja）编译通过；运行 **0 validation 错误**
- 相机空闲时停在默认位 (16.0, 22.6, 16.0)；键盘移动 8.1 单位/秒（速度 8×1s ✓）
- 窗口 resize → swapchain 重建正常，渲染继续
- 同进程两帧像素级一致（`diffRatio = 0`，确定性）
- 重复 `loadMesh(同一路径)` 只上传一次（`AssetLibrary: mesh cache hit (no re-upload)`）
- `../` 相对路径从代码中消失（grep 仅注释）；glfw 仅出现在 `src/platform/`

---

## 二、目标架构与现状对照

官方教程的五层分层（依赖只能向下）：

```
Layer 5  Application   — 组合根、主循环、内容装配（目标：app::App）
Layer 4  Scene         — 纯数据场景、剔除、实例化（目标：scene::）
Layer 3  Rendering     — 帧编排、管线、录制（目标：render::）
Layer 2  Resource      — 资产导入、GPU 资源、句柄（resource::）✅ Phase 2 落地
Layer 1  Platform      — 窗口/输入/日志/路径（platform::）  ✅ Phase 1 落地
```

当前实际放置（`generic/` 为历史遗留，将在 Phase 3/4 拆解）：

| 目标层 | 现状文件 | 说明 |
|---|---|---|
| Layer 1 | `platform/*`、`app/config.hpp` | ✅ 已成型 |
| Layer 2 | `resource/*` | ✅ 已成型 |
| Layer 3 | `renderer/*`、`pipeline_layout.*`、`descriptor_manager.*`、`swapchain.*` | Renderer 仍臃肿，Phase 3 拆分 |
| Layer 4 | `generic/scene.*`、`generic/renderobject.*`、`generic/transform.*` | 仍带 Vulkan 类型，Phase 4 纯数据化 |
| Layer 5 | `c_engine.*` + `main/main.cpp` | CEngine 即 App 雏形 |

---

## 三、模块功能文档

### Layer 1 · 平台抽象（`include/platform/`）

| 模块 | 文件 | 职责 |
|---|---|---|
| `platform::Window` | `window.hpp/cpp` | GLFW 封装：窗口创建、事件钩子（resize/鼠标/光标）、`pollEvents/waitEvents/shouldClose`、framebuffer 尺寸、`requiredInstanceExtensions()`、`createSurface()`、滚动累积（`consumeScrollDelta`）。对外只暴露不透明类型（`GLFWwindow` 仅前向声明） |
| `platform::Input` | `input.hpp/cpp` | 输入门面：`Key`/`MouseButton`/`ButtonAction` 枚举（与 GLFW 解耦）、`poll(Window)`、`isKeyDown`、光标增量、滚动增量 |
| `platform::Log` | `log.hpp/cpp` | 日志抽象：`Log` 接口 + `ConsoleLog`/`NullLog`（Null Object）+ `LogLocator`（全项目唯一 Service Locator，`get()` 永不返回空） |
| `platform::PlatformUtils` | `utils.hpp/cpp` | `assetRoot()`（cwd=bin 时取父目录作工程根）+ `assetPath(rel)`（相对路径解析为绝对路径），消灭 `../` 硬编码 |
| `app::Config` | `app/config.hpp` | 组合根雏形：所有模型/纹理/shader 路径在构造时解析为绝对路径 |

### Layer 2 · 资源管理（`include/resource/`）

| 模块 | 文件 | 职责 |
|---|---|---|
| `resource::MeshData` | `mesh_data.hpp/cpp` | CPU 网格数据（顶点/索引 vector）。`postProcess()`：顶点去重、平滑法线生成、切线计算（原 Mesh 构造器逻辑迁入） |
| `resource::ImageData` | `image_data.hpp` | CPU 图像数据（RGBA8 像素 + 宽高） |
| `resource::MeshImporter` | `mesh_importer.hpp/cpp` | OBJ（tinyobjloader）+ glTF 2.0（tinygltf3，含 sparse accessor/strip/fan 展开）→ `MeshData`。按扩展名分发 |
| `resource::TextureImporter` | `texture_importer.hpp/cpp` | stb_image → `ImageData` |
| `resource::UploadQueue` | `upload_queue.hpp/cpp` | 封装 transient pool 的单次提交 + staging 模板：`uploadBuffer`（staging→device buffer）、`uploadImage`（过渡+拷贝+**mipmap 生成**）。MeshGPU/TextureGPU/Buffer 都走它 |
| `resource::MeshGPU` | `resource_registry.hpp` | GPU 网格半：顶点/索引缓冲（`VmaBuffer`），无 CPU 数据 |
| `resource::TextureGPU` | `resource_registry.hpp` | GPU 纹理半：图像/图像视图/mip 数，无 CPU 像素 |
| `resource::AssetHandle` | `resource_registry.hpp` | 不透明引用计数句柄（id=0 为空句柄 = Null Object），拷贝 retain / 析构 release |
| `resource::ResourceRegistry` | `resource_registry.hpp/cpp` | 持有全部 GPU 资产（id→unique_ptr）、`createMeshGPU/createTextureGPU`、`mesh/texture(handle)` 查找、`unregister(id)`；**内建 1×1 默认纹理**（白/平坦法线，Null Object 回落） |
| `resource::AssetLibrary` | `asset_library.hpp/cpp` | 路径缓存 + 引用计数：`loadMesh/loadImage`（get-or-load，重复加载不重传）、`findMesh/findImage`（未命中返回空句柄）、`retain/release`（归零卸载 GPU 资源）。替换旧 `AssetManager` |

### Vulkan 基础设施（介于 Layer 1–3 之间，Phase 3 将归位）

| 模块 | 文件 | 职责 |
|---|---|---|
| `VulkanDevice` | `vulkandevice.hpp/cpp` | Instance / PhysicalDevice / LogicalDevice 创建、扩展/验证层装配、队列索引（graphics/transfer）、MSAA 采样数探测、`renderContext()` |
| `VmaContext` / `VmaBuffer` / `VmaImage` | `vma_allocator.hpp/cpp` | VMA RAII 封装（移动语义、映射、`mappedData()`），创建失败抛异常 |
| `ResourceFactory` | `resourcefactory.hpp/cpp` | **单例**：图像视图创建、布局过渡、`copyBufferToImage`、格式查询/查找（Phase 3 去单例化为 `RhiFactory`） |
| `CommandPool` | `command_manager.hpp/cpp` | 命令池 + 队列句柄，`beginSingleTimeCommands/endSingleTimeCommands`（单次提交 + fence 等待） |
| `Context` | `context.hpp/cpp` | Debug messenger（验证层回调解耦）+ 窗口 surface |

### Layer 3 · 渲染（`include/renderer/` 等，Phase 3 将拆分）

| 模块 | 文件 | 职责 |
|---|---|---|
| `Renderer` | `renderer/renderer.hpp/cpp` | 帧编排：acquire→UBO 更新→录制→submit（timeline semaphore）→present；resize 重建；MSAA 颜色/深度资源。仍持有管线/描述符/同步对象（Phase 3 拆出） |
| `Swapchain` | `renderer/swapchain.hpp/cpp` | Swapchain 创建/重建/清理、图像视图、MSAA 颜色+深度资源、extent/格式查询 |
| `Pipeline` | `pipeline_layout.hpp/cpp` | 读取 SPIR-V → shader module → 图形管线（动态渲染、深度、混合、push constant） |
| `DescriptorSetLayout` | `descriptor_manager.hpp/cpp` | SPIRV-Reflect 自动反射 shader → descriptor set layout + 池大小估算 |
| `DescriptorPool` / `PerFrameDescriptorSet` | `descriptor_manager.hpp/cpp` | 描述符池分配；每帧 UBO 描述符集（`MAX_FRAMES_IN_FLIGHT` 份） |
| `DescriptorSet`（类） | `descriptor_manager.hpp` | **死代码**，Phase 3 删除 |

### Layer 4 · 场景（`include/generic/`，Phase 4 纯数据化）

| 模块 | 文件 | 职责 |
|---|---|---|
| `Scene` | `generic/scene.hpp/cpp` | 对象列表 + `DrawBatch` 缓存（dirty 标记）。`buildDrawBatches` 目前不真正合并（firstInstance 恒 0） |
| `DrawBatch` | `generic/scene.hpp` | `AssetHandle`（保活）+ `const MeshGPU*`（绘制访问）+ 材质 + 实例缓冲/数量 |
| `RenderObject` | `generic/renderobject.hpp/cpp` | mesh 句柄 + 材质 + 实例数据 + 实例 GPU 缓冲（经 UploadQueue） |
| `Material` | `generic/material.hpp/cpp` | 纹理句柄（空句柄回落注册表默认纹理）+ 描述符集 + RenderFlags |
| `Sampler` | `generic/sampler.hpp/cpp` | vk::raii::Sampler 包装 |
| `Buffer<T>` | `generic/buffer.hpp` | 模板 GPU 缓冲：staging 上传经 `UploadQueue`，设备本地缓冲 |
| `Vertex` / `InstanceData` | `generic/vertex.hpp` | 顶点/实例布局（SNORM 法线/切线量化，`static_assert` 64B 实例） |
| `Transform` | `generic/transform.hpp/cpp` | **死代码**，Phase 4 激活（dirty-flag 缓存模式见官方组件系统章节） |

### 其它

| 模块 | 文件 | 职责 |
|---|---|---|
| `Camera` | `camera.hpp/cpp` | 球坐标相机：`viewMatrix`、轨道旋转（鼠标）、WASD/滚轮（经 `CEngine::updateCamera` 轮询 `platform::Input`） |
| `CEngine`（App 雏形） | `c_engine.hpp` + `engine.cpp` | 组合根：initVulkan 十步装配（device→VMA→factory→context→pool→**AssetLibrary**→sampler→material→DSL→pool→scene→renderer）、`run()` 主循环、场景内容（火星 + 1000 岩石，`std::random_device` 随机分布）、成员析构顺序纪律（池先于 set、库先于句柄） |
| `main` | `main/main.cpp` | 入口：构造 CEngine → run，异常捕获 |

### 外部依赖（`include/extern/` + `src/extern/`）

| 库 | 用途 |
|---|---|
| `stb_image.h` | 纹理解码（仅 TextureImporter 一个 TU 定义实现） |
| `tiny_obj_loader.h` | OBJ 解析 |
| `tiny_gltf_v3.c/h` | glTF 2.0 解析（C API，需 `TINYGLTF3_ENABLE_FS`） |
| `spirv_reflect.c/h` | 着色器反射 |
| `vk_mem_alloc.h` | VMA 分配器 |
| Vulkan SDK / GLFW / KTX / glm / slangc | 系统依赖；slang 着色器经 CMake 自定义命令编译为 `shaders/slang.spv` |

---

## 四、关键数据流

### 资产加载（初始化期）

```
CEngine::initAssetLibrary()
  └─ UploadQueue(transient pool) ── ResourceRegistry(allocator, queue) ── AssetLibrary(registry)

loadMesh(path)   → MeshImporter::load (OBJ/glTF) → MeshData::postProcess → registry.createMeshGPU (UploadQueue 上传)
loadImage(path)  → TextureImporter::load (stb)   → ImageData            → registry.createTextureGPU (UploadQueue 上传+mipmap)
句柄流：AssetHandle(id) → RenderObject/Material 持有 → 引用计数管理生命周期
默认纹理：registry.defaultAlbedo/defaultNormal（惰性创建，永不卸载，Null Object 回落）
```

### 每帧渲染（运行期）

```
CEngine::run
  └─ pollEvents → input.poll → updateCamera(dt) → renderer->drawFrame(batches)
       └─ acquire → updateUniformBuffer(UBO: view/proj/camPos/light) → updateDescriptorSet
          → recordCommandBuffer: 过渡 → beginRendering → bind pipeline/Set0/Set1+push constants
            → 逐 batch: bindVertexBuffers(meshGPU.vertexBuffer, instanceBuffer) + bindIndexBuffer → drawIndexed
          → submit(timeline semaphore) → present → 检查 resize
```

---

## 五、构建与运行

```bash
# 配置（Ninja，与 .vscode/tasks.json 一致）
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build cmake-build-debug --parallel 8

# 运行（cwd 必须是 bin/，路径经 PlatformUtils 按 cwd 解析）
cd bin
VK_LAYER_PATH=$VULKAN_SDK/Bin VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./main.exe
```

- shader：`shaders/shader.slang` → `slangc` 编译 → `shaders/slang.spv`（CMake 自定义命令，随构建执行）
- 验证层：Debug 开 / Release 关（`NDEBUG`）
- `bin/main.exe` 被占用时（进程未退出）链接会失败，先结束进程再构建

---

## 六、已知遗留与债务

1. `ResourceFactory` 仍是单例（Phase 3 去单例化为 `RhiFactory`，构造顺序由 App 装配控制）
2. 两处 `findSupportedFormat`、两处布局过渡实现（Phase 3 合并）
3. `Renderer` 仍持有管线/描述符/同步对象，`recordCommandBuffer` 逻辑冗长（Phase 3 拆 `FrameResources`/`CommandRecorder`/`PipelineSpec`）
4. 死代码：`instanceText`、`DescriptorSet` 类（Phase 3 删）；`Transform`（Phase 4 激活）
5. `VulkanDevice::transferQueueIndex/transferQueue` 已无使用者（Phase 2 后 transient pool 改挂 graphics 队列）
6. `Scene::buildDrawBatches` 不真正合并实例（firstInstance 恒 0）
7. 1000 岩石用 `std::random_device` 种子 → 跨运行画面不可复现（黄金帧对比协议采用"同进程帧差=0"）
8. MSAA 硬编码 4x（Phase 3 的 `RenderSettings` 解决）
9. `generic/` 命名空间未落地；`MAX_FRAMES_IN_FLIGHT` 在 `render_context.hpp`（Phase 3/6 归位）
10. Material 的 RenderFlags 恒为"双纹理全开"，暂无按需开关

---

## 七、下一步

- **Phase 3 · 渲染层拆分**：清理死代码、`FrameResources`/`FrameUniforms`/`CommandRecorder`/`PipelineSpec`/`ShaderManager`/`RenderSettings`，Renderer 只剩编排
- **Phase 4 · 场景层纯数据化**：`RenderItem` 契约、`collectRenderItems`、`SceneObject`、激活 `Transform`（采用官方组件系统章的 dirty-flag 模式）、相机迁入 scene 命名空间、剔除预留
- **Phase 5 · 应用层成形**：`app::App`/`GameLoop`/`CameraController`/`DemoScene`/`Config` 扩充
- **Phase 6 · 工程纪律**：CMake 拆 5 个静态库（依赖方向即链接方向）、命名空间落地、头文件卫生、CI/测试可选
