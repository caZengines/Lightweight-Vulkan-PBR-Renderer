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
| Phase 3 渲染层拆分 | `9f15bdb` | ✅ 完成 | `render::FrameResources/CommandRecorder/PipelineCache/PipelineSpec/ShaderManager/RenderSettings/FrameUniforms`；RhiFactory 去单例化注入；死代码清除 |
| Phase 4 场景层纯数据化 | `73c039a` | ✅ 完成 | `src/scene/` 落地（`scene::Scene/SceneObject/Transform/Camera`）；`Scene::collectRenderItems` 直产 RenderItem（app 桥接删除）；Transform 激活（dirty-flag）；剔除预留按路径追踪目标取消 |
| Phase 5 应用层成形 | 本提交 | ✅ 完成 | `c_engine.hpp` 消失 → `app::App` 组合根 + `GameLoop/CameraController/DemoScene/Config`；灯光/FOV 常量收编 DemoScene；拖拽状态机移交 CameraController；bin/ 自包含（运行时 DLL 落地） |
| Phase 6 构建层与工程纪律 | — | ⏳ 未开始 | CMake 拆 5 个静态库、命名空间落地、头文件卫生 |

**当前工作树**：干净。**构建环境**：VS Code tasks.json 使用 **Ninja** 生成器（`cmake-build-debug` / `cmake-build-release`），Debug + Release 双配置通过。

> 2026-08-27：废除 `include/` 树，全部头文件经 `git mv` 并入 `src/` 与实现同目录（纯移动、零代码改动），编译期 include 根由 `-Iinclude` 改为 `-Isrc`；模块重命名（如 `generic/`→场景层命名）仍按计划留在后续 Phase。

### 已完成的验证（Phase 1/2 证据）

- Debug + Release（Ninja）编译通过；运行 **0 validation 错误**
- 相机空闲时停在默认位 (16.0, 22.6, 16.0)；键盘移动 8.1 单位/秒（速度 8×1s ✓）
- 窗口 resize → swapchain 重建正常，渲染继续
- 同进程两帧像素级一致（`diffRatio = 0`，确定性）
- 重复 `loadMesh(同一路径)` 只上传一次（`AssetLibrary: mesh cache hit (no re-upload)`）
- `../` 相对路径从代码中消失（grep 仅注释）；glfw 仅出现在 `src/platform/`
- Phase 3：Debug + Release 双配置构建通过；带验证层启动渲染循环稳定运行（同基线协议）；resize 路径沿用原时序逻辑（帧间跳过语义保留）
- rhi 迁移：`Swapchain/VulkanDevice` 移入 `src/rhi/` 并命名空间化；`Context` 拆为 `DebugMessenger`+`Surface` 后删除；双配置构建通过 + 渲染循环冒烟复测存活
- Phase 4：`grep -rniE "vulkan|glfw|vk::|VkBuffer" src/scene/*.hpp` 零命中（仅注释提及）；双配置构建通过 + 渲染循环冒烟存活；实例绘制不变（1 火星 + 1000 岩石，identity transform ⇒ 合成矩阵与原数据一致）
- Phase 5：双配置构建通过 + 默认 PATH 冒烟存活（bin/ 已落地 ucrt64 运行时 DLL，修复 `0xC0000139` 加载失败）；行为等价——灯光/投影/输入参数原值迁移，`run()` 只剩循环转发

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

当前实际放置：

| 目标层 | 现状文件 | 说明 |
|---|---|---|
| Layer 1 | `platform/*`、`app/config.hpp` | ✅ 已成型 |
| Layer 2 | `resource/*` | ✅ 已成型 |
| Layer 3 | `render/*`（renderer、frame_resources、command_recorder、pipeline*、shader_manager、descriptor_manager、render_item、instance_buffer 等） | ✅ Phase 3 已拆分：Renderer 只剩编排 |
| Layer 4 | `scene/*` | ✅ Phase 4 已纯数据化：头文件零 Vulkan/GLFW 类型 |
| Layer 5 | `app/*` + `main/main.cpp` | ✅ Phase 5 已成形：`app::App` 组合根 + GameLoop/CameraController/DemoScene/Config |

`generic/` 仅剩 GPU 侧过渡件（material/sampler/vertex），随路径追踪改造一并数据化。

---

## 三、模块功能文档

### Layer 1 · 平台抽象（`src/platform/`）

| 模块 | 文件 | 职责 |
|---|---|---|
| `platform::Window` | `window.hpp/cpp` | GLFW 封装：窗口创建、事件钩子（resize/鼠标/光标）、`pollEvents/waitEvents/shouldClose`、framebuffer 尺寸、`requiredInstanceExtensions()`、`createSurface()`、滚动累积（`consumeScrollDelta`）。对外只暴露不透明类型（`GLFWwindow` 仅前向声明） |
| `platform::Input` | `input.hpp/cpp` | 输入门面：`Key`/`MouseButton`/`ButtonAction` 枚举（与 GLFW 解耦）、`poll(Window)`、`isKeyDown`、光标增量、滚动增量 |
| `platform::Log` | `log.hpp/cpp` | 日志抽象：`Log` 接口 + `ConsoleLog`/`NullLog`（Null Object）+ `LogLocator`（全项目唯一 Service Locator，`get()` 永不返回空） |
| `platform::PlatformUtils` | `utils.hpp/cpp` | `assetRoot()`（cwd=bin 时取父目录作工程根）+ `assetPath(rel)`（相对路径解析为绝对路径），消灭 `../` 硬编码 |
| `app::Config` | `app/config.hpp` | 组合根雏形：所有模型/纹理/shader 路径在构造时解析为绝对路径 |

### Layer 2 · 资源管理（`src/resource/`）

| 模块 | 文件 | 职责 |
|---|---|---|
| `resource::MeshData` | `mesh_data.hpp/cpp` | CPU 网格数据（顶点/索引 vector）。`postProcess()`：顶点去重、平滑法线生成、切线计算（原 Mesh 构造器逻辑迁入） |
| `resource::ImageData` | `image_data.hpp` | CPU 图像数据（RGBA8 像素 + 宽高） |
| `resource::MeshImporter` | `mesh_importer.hpp/cpp` | OBJ（tinyobjloader）+ glTF 2.0（tinygltf3，含 sparse accessor/strip/fan 展开）→ `MeshData`。按扩展名分发 |
| `resource::TextureImporter` | `texture_importer.hpp/cpp` | stb_image → `ImageData` |
| `resource::UploadQueue` | `upload_queue.hpp/cpp` | 封装 transient pool 的单次提交 + staging 模板：`uploadBuffer`（staging→device buffer）、`uploadImage`（过渡+拷贝+**mipmap 生成**）。自持 `VmaAllocator`（构造注入，调用方不再逐次传递）。MeshGPU/TextureGPU/InstanceBuffer 都走它 |
| `resource::MeshGPU` | `resource_registry.hpp` | GPU 网格半：顶点/索引缓冲（`VmaBuffer`），无 CPU 数据 |
| `resource::TextureGPU` | `resource_registry.hpp` | GPU 纹理半：图像/图像视图/mip 数，无 CPU 像素 |
| `resource::AssetHandle` | `resource/asset_handle.hpp` | 不透明引用计数句柄（id=0 为空句柄 = Null Object），拷贝 retain / 析构 release。独立 CPU 侧头文件：跨层包含不牵连 GPU 声明 |
| `resource::ResourceRegistry` | `resource_registry.hpp/cpp` | 持有全部 GPU 资产（id→unique_ptr）、`createMeshGPU/createTextureGPU`、`mesh/texture(handle)` 查找、`unregister(id)`；**内建 1×1 默认纹理**（白/平坦法线，Null Object 回落） |
| `resource::AssetLibrary` | `asset_library.hpp/cpp` | 路径缓存 + 引用计数：`loadMesh/loadImage`（get-or-load，重复加载不重传）、`findMesh/findImage`（未命中返回空句柄）、`retain/release`（归零卸载 GPU 资源）。替换旧 `AssetManager` |

### rhi 层基础设施（`src/rhi/`，Phase 3 起全部归位；根级仅余 `command_manager.*`、`render_context.hpp` 待 Phase 6 收尾）

| 模块 | 文件 | 职责 |
|---|---|---|
| `rhi::VulkanDevice` | `rhi/vulkan_device.hpp/cpp` | Instance / PhysicalDevice / LogicalDevice 创建、扩展/验证层装配、队列索引、MSAA 采样数探测、`renderContext()` |
| `rhi::VmaContext` / `VmaBuffer` / `VmaImage` | `rhi/vma_allocator.hpp/cpp` | VMA RAII 封装（移动语义、映射、`mappedData()`） |
| `rhi::RhiFactory` | `rhi/rhi_factory.hpp/cpp` | 非单例：图像视图、sync2 屏障核心、上传路径 sync1 过渡包装、格式探测 |
| `rhi::DebugMessenger` | `rhi/debug_messenger.hpp/cpp` | 验证层回调节耦（原 Context 拆分件之一，按开关惰性创建） |
| `rhi::Surface` | `rhi/surface.hpp/cpp` | 窗口表面（原 Context 拆分件之二，经 `Window::createSurface` 保持 GLFW 封闭在 platform/） |
| `rhi::Swapchain` | `rhi/swapchain.hpp/cpp` | Swapchain + MSAA color/depth 资源；注入 RhiFactory；深度格式单一探测定点；surface 引用 const 正确化 |
| `rhi::CommandPool` | `command_manager.hpp/cpp`（根级，暂留） | 命令池 + 队列句柄，单次提交辅助 |

### Layer 3 · 渲染（`src/render/`，Phase 3 完成拆分）

| 模块 | 文件 | 职责 |
|---|---|---|
| `render::Renderer` | `render/renderer.hpp/cpp` | **仅帧编排**：`beginFrame()`（等 fence + acquire + UBO/Set0 更新）→ `record(ctx, span<RenderItem>)` → `endFrame()`（timeline submit + present）；swapchain 重建编排；持有 Deps 聚合注入 |
| `render::FrameResources` | `render/frame_resources.hpp/cpp` | per-frame 状态唯一属主：UBO 缓冲、Set0 描述符集、命令缓冲、binary+timeline 同步对象；`kMaxFramesInFlight` 常量迁入 |
| `render::CommandRecorder` | `render/command_recorder.hpp/cpp` | 录制：布局过渡 → beginRendering → 绑定管线/Set0/逐项 Set1+push flags+实例绘制；输入 `std::span<const RenderItem>` |
| `render::RenderItem` | `render/render_item.hpp` | 场景↔渲染的扁平 POD 契约（mesh/material 指针 + InstanceBuffer 指针 + 实例段），无 Vulkan 类型；Phase 4 起 `Scene::collectRenderItems` 直产 |
| `render::InstanceBuffer` | `render/instance_buffer.hpp/cpp` | 单个 SceneObject 的 GPU 实例流（vertex binding 1，每实例一个 model 矩阵）；经 UploadQueue 一次性上传。过渡形态：路径追踪管线将以 TLAS instance 取代 |
| `render::GraphicsPipelineSpec` / `PipelineCache` | `render/pipeline_spec.hpp`、`pipeline_cache.hpp/cpp` | 管线状态 POD 化 + 按 spec 缓存创建（原硬编码消除） |
| `render::ShaderManager` | `render/shader_manager.hpp/cpp` | SPIR-V 加载 + 缓存 + shader module 创建（反射输入与建管线共享一次磁盘读） |
| `render::DescriptorSetLayout/Pool` | `render/descriptor_manager.hpp/cpp` | SPIRV-Reflect 自动布局与池大小估算；死代码 `DescriptorSet` 类、旧 `PerFrameDescriptorSet` 已删（职能并入 FrameResources） |
| `render::FrameUniforms` | `render/frame_uniforms.hpp` | `UniformBufferObject/Light` 唯一定义处（含 static_assert 布局校验） |
| `render::RenderSettings` | `render/render_settings.hpp` | MSAA 采样数（Config 注入并按设备上限钳制）与 present mode 偏好，替代硬编码 |

### Layer 4 · 场景（`src/scene/`，Phase 4 纯数据化完成）

头文件零 Vulkan/GLFW 类型：接口只出现 GLM 数学、`resource::AssetHandle`、`render::RenderItem/InstanceBuffer`（均为无 Vulkan 依赖的头）。

| 模块 | 文件 | 职责 |
|---|---|---|
| `scene::Scene` | `scene/scene.hpp/cpp` | 对象列表属主；`collectRenderItems()` 直产 `std::span<const render::RenderItem>`（dirty 缓存，帧间零分配、零拷贝） |
| `scene::SceneObject` | `scene/scene_object.hpp/cpp` | 纯数据可绘制体：mesh 句柄（保活）+ 材质 + `Transform` + 实例放置（CPU 本地副本）。`setInstances` 经 UploadQueue 上传 `render::InstanceBuffer`，world = transform × instance |
| `scene::Transform` | `scene/transform.hpp/cpp` | TRS 结构 + `toMatrix()`；`SceneObject::worldMatrix()` 以 dirty-flag 惰性缓存（原死代码激活） |
| `scene::Camera` | `scene/camera.hpp/cpp` | 球坐标相机纯数学：`viewMatrix/position/orbit/moveHorizontal/moveVertical/zoom`；输入接线（鼠标拖拽状态机）留在应用边界 |
| `Material` | `generic/material.hpp/cpp` | 过渡件：纹理句柄（空句柄回落注册表默认纹理）+ 描述符集 + RenderFlags。Set-1 现按材质分配一次（引擎 `initMaterialDescriptors`），路径追踪时数据化为 BSDF 参数 |
| `Sampler` | `generic/sampler.hpp/cpp` | vk::raii::Sampler 包装 |
| `Vertex` / `InstanceData` | `generic/vertex.hpp` | 纯数据顶点/实例布局（SNORM 量化，`static_assert` 64B 实例）；Vulkan 输入布局描述已移入 `pipeline.cpp`（数据与管线关注点分离） |

### Layer 5 · 应用（`src/app/`，Phase 5 完成）

| 模块 | 文件 | 职责 |
|---|---|---|
| `app::App` | `app/app.hpp/cpp` | **组合根**：构造函数按层装配（window→rhi→resources→samplers→content→render），`run()` 只做循环转发；`~App` 清理；成员析构逆序纪律（DescriptorPool 声明先于一切持 set 成员） |
| `app::GameLoop` | `app/game_loop.hpp/cpp` | 帧节奏：pollEvents → input.poll → deltaTime → update/render 回调；固定步长骨架预留 |
| `app::CameraController` | `app/camera_controller.hpp/cpp` | 输入→相机映射：左键拖拽轨道状态机（相机保持纯数学）、WASD/Space/Shift、滚轮 |
| `app::DemoScene` | `app/demo_scene.hpp/cpp` | 示例内容：三材质 + 火星 + 1000 随机岩石；持有 `FrameParams`（灯光/FOV/远近平面，原 renderer 字面量收编） |
| `app::Config` | `app/config.hpp` | 全部内容参数：窗口尺寸/标题、验证层与设备扩展、MSAA、present mode、资产路径（构造时解析为绝对路径） |

### 其它

| 模块 | 文件 | 职责 |
|---|---|---|
| `main` | `main/main.cpp` | 入口：构造 `app::App` → run，异常捕获 |

### 外部依赖（`src/extern/`，头文件与实现单树共存）

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
  └─ UploadQueue(transient pool, allocator) ── ResourceRegistry(allocator, queue) ── AssetLibrary(registry)

loadMesh(path)   → MeshImporter::load (OBJ/glTF) → MeshData::postProcess → registry.createMeshGPU (UploadQueue 上传)
loadImage(path)  → TextureImporter::load (stb)   → ImageData            → registry.createTextureGPU (UploadQueue 上传+mipmap)
句柄流：AssetHandle(id) → SceneObject/Material 持有 → 引用计数管理生命周期
默认纹理：registry.defaultAlbedo/defaultNormal（惰性创建，永不卸载，Null Object 回落）

场景装配（初始化期，Phase 4 起）
SceneObject(mesh handle, material, registry)
  └─ setInstances(queue, locals)  // world = transform × instance → render::InstanceBuffer 一次性上传
材质 Set-1：initMaterialDescriptors() 按材质各分配一次（共享材质不重复分配）
```

### 每帧渲染（运行期）

```
app::GameLoop::run
  ├─ pollEvents → input.poll → CameraController.update(dt)（WASD/滚轮/拖拽 → scene::Camera）
  ├─ renderer->beginFrame()                    // 空 optional ⇒ 本帧跳过（swapchain 重建中）
  │    └─ waitFence → acquire(OOD⇒recreate) → fillUBO(camera + FrameParams) → writeSet0 → resetFence/cmdBuffer
  ├─ renderer->record(*frame, scene_.collectRenderItems())   // Scene 直产（dirty 缓存），CommandRecorder 过渡→Rendering→逐 item 绘制
  └─ renderer->endFrame(*frame)                // timeline+binary submit → present → resize/OOD 处理
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
- bin/ 自包含：CMake POST_BUILD 拷贝 ucrt64 运行时 DLL（libstdc++/libgcc/libwinpthread/glfw3）——PATH 上 mingw64/bin 先于 ucrt64/bin，不落地会 `0xC0000139` 启动即崩
- `bin/main.exe` 被占用时（进程未退出）链接会失败，先结束进程再构建

---

## 六、已知遗留与债务

1. `Material` 仍是 GPU 侧过渡件（`generic/`，含描述符集/RenderFlags）；路径追踪改造时数据化为 BSDF 参数表，`generic/` 剩余件（material/sampler/vertex）随之一并收编
2. `SceneObject::setTransform` 后需重新 `setInstances` 才会更新 GPU 实例流（静态场景下无影响）；`SceneNode` 父子层次按计划暂缓——层次结构届时由 TLAS/BLAS 场景装配承接，`worldMatrix()`（dirty-flag）已就位
3. `VulkanDevice::transferQueueIndex/transferQueue` 已无使用者（Phase 2 后 transient pool 改挂 graphics 队列）
4. 1000 岩石用 `std::random_device` 种子 → 跨运行画面不可复现（黄金帧对比协议采用"同进程帧差=0"）
5. Material 的 RenderFlags 恒为"双纹理全开"，暂无按需开关
6. `VULKAN_HPP_*` 宏仍在各 TU 头部重复定义，待收敛到单一公共头（工程纪律项）；根级 `command_manager.*`、`render_context.hpp` 待 Phase 6 归位
7. bin/ 依赖 CMake POST_BUILD 落地 ucrt64 运行时 DLL（libstdc++/libgcc/libwinpthread/glfw3）——PATH 上 mingw64/bin 先于 ucrt64/bin，不落地会 `0xC0000139` 启动即崩

---

## 七、下一步

- **Phase 6 · 工程纪律**（重构收官）：CMake 拆静态库（platform → resource → render → scene → app，链接方向即依赖方向）、`GLOB_RECURSE` 换显式列表、`VULKAN_HPP_*` 宏收编单一头、根级 `command_manager.*`/`render_context.hpp` 归位、头文件卫生、可选 CI/测试
- **重构完成后 · 光线追踪管线**（见计划文档 §6 路线图）：NVIDIA GPU 上启用 `VK_KHR_acceleration_structure`/`VK_KHR_ray_tracing_pipeline`，BLAS/TLAS 由 `SceneObject` 数据（mesh + worldMatrix）构建，`RenderItem` 数据面直接复用；最终形态为纯路径追踪离线渲染器
- 代理/协作者入口：仓库根 `AGENTS.md`（构建运行、模块地图、工程约定）
