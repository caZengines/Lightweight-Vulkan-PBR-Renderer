# 五层架构重构方案 —— 面向 Vulkan PBR 离线渲染器

> **依据**：Vulkan 官方教程《Building a Simple Engine》的
> [Engine Architecture: Introduction](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/01_introduction.html)、
> [Architectural Patterns](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/02_architectural_patterns.html)、
> [Component Systems](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/03_component_systems.html)、
> [Rendering Pipeline](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/05_rendering_pipeline.html) 章节，
> 以及 [Appendix](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Appendix/appendix.html) 中关于 Layered Architecture 的说明。
>
> **核心原则（来自教程）**：分层架构（Layered Architecture）中，**依赖只允许向下流动**——每一层只能使用它下方各层提供的服务，通过明确定义的接口交互；把 API 相关代码压到最底层，上层保持 API 无关。这样引擎可移植、可测试、可维护。
>
> **总目标**：在不改变渲染结果的前提下，把当前"CEngine 上帝类 + Renderer 上帝类"的单体结构，渐进重构为 5 层架构，并且**每一步之后项目都能编译、运行、画面不变**。

---

## 0. 结论速览（TL;DR）

| 维度 | 现状 | 目标 |
|---|---|---|
| 平台抽象 | GLFW 泄漏到 5+ 个文件（Renderer 里直接 `glfwGetKey`） | 仅 `platform/` 子层接触 GLFW |
| 资源管理 | `AssetManager` 只做路径缓存；Mesh/Texture 把"文件解析 + GPU 上传"揉在一个类里 | `AssetLibrary`（CPU 资产）+ `ResourceRegistry`（GPU 句柄）+ `UploadQueue` |
| 渲染层 | `Renderer` 同时干帧循环、输入、相机、灯光、录制、present 的活 | `Renderer` 只管帧编排；`CommandRecorder`/`FrameResources`/`PipelineCache`/`ShaderManager` 分而治之 |
| 场景层 | `DrawBatch` 含 `VkBuffer`、`RenderObject` 持有 GPU 实例缓冲 | 纯数据场景 + `RenderItem` 接口，场景层零 Vulkan/GLFW 类型 |
| 应用层 | `CEngine` 既是启动器又是"游戏内容"（1000 块石头写死在代码里） | `App`（组合根）+ `GameLoop` + `CameraController` + 内容独立 |
| 构建 | 单可执行目标 + `GLOB_RECURSE`，分层无法被构建系统强制 | 5 个静态库目标，`target_link_libraries` 强制依赖方向 |

建议按 **Phase 0 → 6** 渐进执行，每阶段独立可验证（详见第 4 节）。

---

## 1. 现状分析

### 1.1 模块清单与职责

| 类 / 文件 | 职责 | 备注 |
|---|---|---|
| `CEngine`（include/c_engine.hpp, src/engine.cpp） | 窗口创建与 GLFW 回调、Vulkan 初始化编排、命令池、资产加载、采样器/材质/描述符布局/池、场景搭建（1000 石头+火星）、每帧循环 | 17 个成员、10 步初始化、4 个静态回调 —— 上帝类 |
| `VulkanDevice` | Instance / PhysicalDevice / Device / 队列 / MSAA；`renderContext()` 打包上下文 | 依赖 GLFW 取扩展（vulkandevice.cpp:59） |
| `VmaContext` / `VmaBuffer` / `VmaImage` | VMA 分配器与 RAII 资源包装 | 质量好，直接保留 |
| `Context` | Debug messenger + Window surface | 名字太泛；surface 创建依赖 GLFW（context.cpp:44） |
| `CommandPool` | 命令池 + 队列 + 单次提交辅助 | 直接保留 |
| `ResourceFactory` | 单例；ImageView、拷贝、布局转换、格式查询 | 与 `Pipeline::findSupportedFormat`、`Renderer::transition_image_layout` 重复 |
| `AssetManager` | 按路径缓存 Texture/Mesh | 无引用计数/卸载/异步；glTF 未接入 |
| `DescriptorSetLayout` / `DescriptorPool` / `PerFrameDescriptorSet` / `DescriptorSet` | SPIRV-Reflect 自动布局 + 池 + 集合 | 反射部分很有价值；`DescriptorSet` 类是死代码 |
| `Pipeline` | 读 SPIR-V、建管线（状态全部硬编码）、深度格式查询 | 硬编码 `../shaders/slang.spv` 与全套状态 |
| `Swapchain` | swapchain + MSAA 颜色/深度资源 + ImageView | 持有 `GLFWwindow*` |
| `Renderer` | 帧循环：acquire → UBO → 描述符 → 录制（转换+渲染+绘制）→ submit → present；resize；**输入轮询**；**灯光常量** | 上帝类二号 |
| `Scene` / `RenderObject` / `DrawBatch` | 对象列表、批缓存、实例缓冲 | `DrawBatch` 含 `VkBuffer`；`RenderObject` 含 GPU 缓冲 |
| `Mesh` / `Texture` / `Material` / `Sampler` / `Buffer<T>` | GPU 资源类型；OBJ 解析 + 去重 + 法线/切线生成；stb 贴图 + mipmap | 解析与上传耦合在构造器里 |
| `glTFModel`（glFTloader） | tinygltf3 解析 glTF 几何 | **未接入 AssetManager**，游离模块 |
| `Camera` | 轨道相机 + 鼠标/WASD 输入 | 输入语义耦合 GLFW 常量 |
| `Transform` | TRS 结构 | **死代码**（未被引用） |
| `Vertex` / `InstanceData` | 顶点布局 + 实例布局 | 绑定描述用于管线创建，属 RHI 概念 |
| `main.cpp` | 建 CEngine、run | 简单，没问题 |

### 1.2 关键依赖关系（现状）

```
main → CEngine
CEngine → GLFW | VulkanDevice | VmaContext | Context | CommandPool | AssetManager
        → Sampler | DescriptorSetLayout | DescriptorPool | Material | Scene | Renderer | Camera
Renderer → GLFW(window) | Camera | Scene(DrawBatch) | CommandPool | Swapchain | Pipeline
         → PerFrameDescriptorSet | VmaBuffer(UBO) | DescriptorPool/Layout
Scene/RenderObject → Mesh | Material | Buffer<InstanceData>(Vulkan 类型)
Mesh/Texture → ResourceFactory(单例) | VmaBuffer/VmaImage | CommandPool
Pipeline → Vertex/InstanceData(顶点布局) | RenderContext
```

特征：**顶层直接持有所有东西**（CEngine 17 个成员），**横向依赖密集**，**不存在任何抽象边界**——任何类都可以 include 任何头文件，编译期没有任何约束。

### 1.3 每帧数据流（现状）

```
CEngine::run()
  ├─ scene_.getDrawBatches()           // 循环外取一次 const 引用（缓存）
  └─ while(!glfwWindowShouldClose)
       ├─ glfwPollEvents()             // GLFW 回调 → Camera（鼠标）/ Renderer.framebufferResized
       ├─ renderer.drawFrame(batches)
       │    ├─ waitForFences / acquireNextImage（out-of-date → recreateAfterResize）
       │    ├─ updateUniformBuffer → updateCamera() → glfwGetKey 轮询键盘 ← 渲染层在做输入
       │    ├─ updateDescriptorSet
       │    ├─ recordCommandBuffer：3 次布局转换 → beginRendering → 逐 batch
       │    │     bind pipeline / set0(UBO) / set1(材质) / pushConstants(flags) / 实例绘制
       │    ├─ submit（timeline semaphore + binary semaphores + fence）
       │    └─ presentKHR（out-of-date/suboptimal/resized → recreateAfterResize）
       └─ (无 update 阶段；相机/灯光全在渲染层内部更新)
```

### 1.4 主要架构问题（含证据）

按严重程度排序：

1. **没有平台抽象层，GLFW 泄漏到 5 个文件**
   - `Renderer::updateCamera()` 直接 `glfwGetKey`（renderer.cpp:313-330）——**渲染层在轮询键盘**；
   - `Swapchain` 持有 `GLFWwindow*` 并调 `glfwGetFramebufferSize`（swapchain.cpp:55, 113-115）；
   - `VulkanDevice::GetRequiredExtension()` 调 `glfwGetRequiredInstanceExtensions`（vulkandevice.cpp:59）；
   - `Context::createSurface()` 调 `glfwCreateWindowSurface`（context.cpp:44）；
   - `CEngine` 的 4 个静态回调以 GLFW 类型为参数（c_engine.hpp:75-78）；
   - `Camera` 输入语义耦合 `GLFW_MOUSE_BUTTON_LEFT` 等常量（camera.cpp:11）。
   → 换窗口库/换平台时，渲染层、设备层、场景层全部要改。

2. **Renderer 是"渲染层上帝类"**（renderer.hpp / renderer.cpp，380 行）
   帧循环、同步对象、UBO、描述符、命令录制、**输入**、**相机**、**灯光**、present、resize 全在一个类里。
   对外依赖 7 类对象（GLFW window、Camera、Scene 批、DescriptorPool/Layout、CommandPool、VmaAllocator、Surface）。
   → 无法单独测试录制逻辑；无法加第二个 pass（如 path tracing）而不动这个类。

3. **场景层携带 Vulkan 类型**
   - `DrawBatch` 含 `VkBuffer instanceBuffer`（scene.hpp:8）；
   - `RenderObject` 持有 GPU 实例缓冲并负责 `initMaterialDescriptor`（renderobject.hpp:19-22）——场景对象 = GPU 对象；
   - 批缓存 `cachedBatches_`（scene.hpp:26）是渲染侧概念。
   → 场景层被绑死在 Vulkan 上，做空间分区/剔除时无处下手（README 中"Frustum Culling"未实现与此直接相关）。

4. **资源管理与 GPU 资源纠缠**
   - `Mesh` 构造器同时做 CPU 处理（去重/法线/切线生成）和 GPU 上传（mesh.cpp:6-133）；
   - `Texture::createTexture` 文件 IO + staging + mipmap + ImageView 全在一个函数（texture.cpp:78-123）；
   - `AssetManager` 只是"路径→shared_ptr"的 map（asset_manager.cpp），无引用计数语义、无卸载、无异步、无默认资源管理；
   - `glTFModel` 已实现但**未接入 AssetManager**（asset_manager.cpp 只有 OBJ）；
   - 默认 1×1 贴图在主程序里手工创建（engine.cpp:126-129）。
   → 无法实现流式加载/热重载/多线程上传；glTF 支持悬空。

5. **应用层与引擎启动混为一体**
   - `CEngine::initScene()` 里硬编码场景内容：1000 块石头的随机分布、火星位置/缩放（engine.cpp:162-214）；
   - 灯光、相机 FOV、投影参数硬编码在 `Renderer::updateUniformBuffer`（renderer.cpp:290-306）；
   - 常量散落全局：`WIDTH/HEIGHT`、`validationLayers`、`requiredDeviceExtensions`（c_engine.hpp:10-23）、资源路径（context.hpp:13-19）、`MAX_FRAMES_IN_FLIGHT`（render_context.hpp）。
   → 换一个"游戏"必须改引擎代码；离线渲染器未来做多场景/多相机（path tracing 视角）没有落点。

6. **单例与全局可变状态（Service Locator 视角）**
   - `ResourceFactory` 是"裸单例 + 裸指针"（resourcefactory.hpp:15-27）：没有抽象接口（调用方直接耦合具体类）、`init()` 幂等但不可重入、初始化顺序（GPP 所谓 temporal coupling）脆弱——正是《Service Locator》章节指出的单例三大问题；
   - 无日志服务：`std::cout/cerr` 散落 vulkandevice.cpp:87、asset_manager.cpp:20、renderer.cpp:150-156、context.cpp:26，无处统一开关/过滤；
   - `inline int instanceText = 1;`（renderer.hpp:22）——未使用的全局变量。

7. **重复实现**
   - `ResourceFactory::transitionImageLayout`（同步 1 风格）vs `Renderer::transition_image_layout`（同步 2 风格）两套布局转换；
   - `ResourceFactory::findSupportedFormat` vs `Pipeline::findSupportedFormat`；
   - `VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS` / `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS` 在 6+ 个头文件重复 `#define`。

8. **CWD 相对路径**
   `"../models/..."`（context.hpp:13-19）、`"../shaders/slang.spv"`（pipeline_layout.cpp:17, engine.cpp:86）
   → 必须从固定工作目录启动，无"资源根"抽象；这是资源层该解决的第一个问题。

9. **死代码**
   `Transform`（transform.hpp/cpp，从未使用）、`DescriptorSet` 包装类（material.cpp:24 直接 `allocateDescriptorSets`）、`instanceText`、`getObjects()`/`findMesh()` 等未用接口。

10. **构建层无法约束架构**
    - `file(GLOB_RECURSE SRC_LIST ...)`（CMakeLists.txt:29-33）：新增文件后必须重新 configure，且无法表达模块边界；
    - `context.cpp:1`、`camera.cpp:1` 直接 `#include "c_engine.hpp"`——头文件包含卫生差（include-what-you-use 违规），增大编译面；
    - `generic/` 目录语义模糊：场景（scene）、GPU 资源（mesh/texture/material）、数学（transform）混放。

11. **小问题（顺手修）**
    - `Scene::buildDrawBatches` 注释声称"合并相同 (Material, Mesh) 实例"，实际每个 RenderObject 独立成批、`firstInstance` 恒为 0（scene.cpp:3-20）；
    - MSAA 硬编码 4x（vulkandevice.cpp:227）；
    - `run()` 在循环外取批列表 `const auto&`（engine.cpp:59），运行期场景变更不会被感知；
    - `Swapchain::cleanupSwapChain` 不显式销毁 color/depth 图像（依赖 VmaImage 成员重赋值时的 RAII）。

---

## 2. 目标五层架构设计

### 2.1 分层总览与依赖规则

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 5  Application    App | GameLoop | CameraController |      │
│                          DemoScene(内容) | Config                │
├─────────────────────────────────────────────────────────────────┤
│ Layer 4  Scene          Scene | SceneNode | SceneObject |        │
│                          Camera(纯数学) | Frustum/SpatialIndex   │
│                          RenderItem 生产者                      │
├─────────────────────────────────────────────────────────────────┤
│ Layer 3  Rendering      Renderer | FrameResources |              │
│                          CommandRecorder | BatchBuilder |        │
│                          PipelineCache/PipelineSpec |            │
│                          ShaderManager | DescriptorManager |     │
│                          MaterialInstance | RenderItem 消费者    │
├─────────────────────────────────────────────────────────────────┤
│ Layer 2  Resource       AssetLibrary | MeshImporter/TextureImporter│
│                          MeshData/ImageData(CPU 资产) |          │
│                          ResourceRegistry(GPU 句柄) |            │
│                          MeshGPU/TextureGPU | UploadQueue |      │
│                          Buffer<T> | Sampler                    │
├─────────────────────────────────────────────────────────────────┤
│ Layer 1  Platform       platform: Window | Input | PlatformUtils │
│                          rhi: VulkanDevice | VmaContext/VmaBuffer│
│                          /VmaImage | CommandPool | RhiFactory |  │
│                          DebugMessenger | Surface | Swapchain |  │
│                          RenderContext | Vertex/InstanceData     │
└─────────────────────────────────────────────────────────────────┘
         ↑ 依赖只允许向下（App 作为组合根可引用所有层）
```

**依赖规则**（对齐教程 Architectural Patterns 的 Layered Architecture 说明）：

| 规则 | 说明 |
|---|---|
| R1 依赖向下 | 第 N 层只能依赖 ≤N 的层；禁止向上依赖 |
| R2 无环 | 层间不得成环；循环依赖 = 设计错误 |
| R3 类型隔离 | `vulkan/*`、`GLFW` 头文件**禁止出现在 Layer 2 以上的头文件里**（Layer 2/3 的 .cpp 可经 Layer 1 接口使用 RHI，但类型经句柄/不透明结构传递） |
| R4 接口契约 | 层间只通过公开接口交互；`RenderItem`、`AssetHandle`、`FrameContext` 就是契约 |
| R5 组合根例外 | `App`（Layer 5）负责装配所有层，允许引用任意层 |

> 关于 Scene 与 Rendering 的次序：按你定义的分层，**Scene(4) 在上、Rendering(3) 在下**，即场景层"生产" `RenderItem`，渲染层"消费"。这是合理的（场景知道"画什么"，渲染知道"怎么画"）。另一种常见做法是渲染层在上、主动从场景拉取（renderer 拉模型），未来若做多渲染后端再调换即可，接口 `collectRenderItems` 不变。

### 2.2 各层核心类/模块

#### Layer 1 · Platform Abstraction（平台抽象层）

| 模块 | 来源 | 职责 |
|---|---|---|
| `platform::Window` | **新**（吸收 CEngine 窗口部分 + Swapchain/Context/VulkanDevice 里的 GLFW 调用） | 封装 GLFW 窗口：创建/轮询/关闭查询/帧缓冲尺寸/`requiredInstanceExtensions()`/`createSurface(instance)`；以 `std::function` 回调替代 `framebufferResized` 标志 |
| `platform::Input` | **新**（吸收 Renderer::updateCamera 轮询 + Camera 的 GLFW 常量） | `Key`/`MouseButton` 枚举（替代 `GLFW_KEY_*`/`GLFW_MOUSE_BUTTON_*`）；`isKeyDown`/`isMouseDown`/`cursorDelta`/`scrollDelta` |
| `platform::PlatformUtils` | **新** | 时间（deltaTime）、资源根路径解析、日志 stub |
| `rhi::VulkanDevice` | 现有 `VulkanDevice`（迁入） | 去 GLFW：扩展由 `Window::requiredInstanceExtensions()` 提供 |
| `rhi::VmaContext/VmaBuffer/VmaImage` | 现有 | 原样保留 |
| `rhi::CommandPool` | 现有 | 原样保留 |
| `rhi::RhiFactory` | 现有 `ResourceFactory` 改造 | **去单例化**：作为 RHI 上下文成员、构造器注入给上层（GPP Service Locator："优先显式传参"）；合并两套布局转换、两处 `findSupportedFormat` |
| `platform::Log` | **新** | 日志服务定位器：`Log` 接口 + `ConsoleLog`/`NullLog` 提供者 + `LogLocator::provide/get`；收编散落的 std::cout/cerr（详见 §2.5.2） |
| `rhi::DebugMessenger` / `rhi::Surface` | 现有 `Context` 拆分 | debug messenger 与 surface 各自独立 |
| `rhi::Swapchain` | 现有（迁入） | 依赖 `Window&` 而非 `GLFWwindow*`；补上 color/depth 显式清理 |
| `rhi::RenderContext` | 现有 | 保留；`MAX_FRAMES_IN_FLIGHT` 迁到 `render` 层 |
| `rhi::Vertex` / `rhi::InstanceData` | 现有 `generic/vertex.hpp` | 顶点布局描述（RHI 概念） |

#### Layer 2 · Resource Management（资源管理层）

| 模块 | 来源 | 职责 |
|---|---|---|
| `resource::AssetLibrary` | 现有 `AssetManager` 改造 | `AssetId`/`AssetHandle<T>` 句柄化缓存；`load<MeshData>/load<ImageData>`；引用计数、`unload`、`releaseUnused`；路径经 `PlatformUtils::assetPath` 解析 |
| `resource::MeshImporter` | 拆自 `Mesh::fromObj` + `glTFModel` | OBJ（tinyobj）/ glTF（tinygltf3）→ 纯 CPU `MeshData`；**把 glTF 接入资产库** |
| `resource::TextureImporter` | 拆自 `Texture::createTexture` | stb_image / KTX → 纯 CPU `ImageData`（含导入选项：格式、sRGB） |
| `resource::MeshData` / `ImageData` | **新** | CPU 侧资产结构（顶点/索引/像素）；去重、法线/切线生成逻辑迁入 MeshData 处理函数 |
| `resource::ResourceRegistry` | **新** | `MeshGpuHandle`/`TextureGpuHandle` → `MeshGPU`/`TextureGPU`（VmaImage+View+mip）；创建/查询/销毁；默认资源（白贴图/平坦法线）内建 |
| `resource::UploadQueue` | 改造 `transientCommandPool` 的用法 | 封装 staging + 单次命令提交；供注册表与 `Buffer<T>` 使用 |
| `resource::Buffer<T>` / `Sampler` | 现有（迁入） | GPU 缓冲模板与采样器；`Buffer<T>` 改走 `UploadQueue` |

#### Layer 3 · Rendering（渲染层）

| 模块 | 来源 | 职责 |
|---|---|---|
| `render::Renderer` | 现有瘦身 | 只留帧编排：`beginFrame → record → endFrame`；swapchain 重建订阅 `Window::onFramebufferResize`；同步对象 |
| `render::FrameResources` | **新**（拆自 Renderer） | per-frame UBO 缓冲、per-frame 描述符集、命令缓冲、fence/semaphore（binary + timeline） |
| `render::FrameUniforms` | 拆自 renderer.hpp | `UniformBufferObject`/`Light` 结构（renderer.hpp:24-36 迁出），删除 `instanceText` |
| `render::CommandRecorder` | **新**（拆自 Renderer::recordCommandBuffer） | 布局转换 + beginRendering + 绑定 + 绘制；接收 `std::span<const RenderItem>` |
| `render::BatchBuilder` | 改造 `Scene::buildDrawBatches` | 按 (mesh, material) 合并 RenderItem → 批；移到渲染侧，保留 dirty 缓存 |
| `render::PipelineSpec` + `PipelineCache` | 改造 `Pipeline` | 管线状态参数化（消除硬编码）；按 spec 缓存 `GraphicsPipeline` |
| `render::ShaderManager` | 改造 `Pipeline` 的读文件 + `DescriptorSetLayout` 的反射 | 加载 SPIR-V（路径来自配置）、创建 ShaderModule、SPIRV-Reflect 生成布局描述并缓存 |
| `render::DescriptorManager` | 现有 `DescriptorSetLayout/DescriptorPool/PerFrameDescriptorSet` 整合 | 删死代码 `DescriptorSet`；池大小策略保留（Set0 × MAX_FRAMES_IN_FLIGHT，Set1+ × 对象数） |
| `render::MaterialInstance` | 现有 `Material`（迁入） | 材质参数（引 `TextureGpuHandle`）+ 描述符集；`RenderFlags` 保留 |
| `render::MeshGPU` / `TextureGPU` | 拆自现有 `Mesh`/`Texture` 的 GPU 半 | 纯 GPU 对象（缓冲/图像+视图），生命周期归注册表 |
| `render::RenderSettings` | **新** | MSAA（替代 vulkandevice.cpp:227 的硬编码 4）、present mode、分辨率、垂直同步 |
| `render::RenderItem` | **新**（接口契约） | 场景层填、渲染层吃的纯数据（见 2.3） |

#### Layer 4 · Scene Management（场景管理层）

| 模块 | 来源 | 职责 |
|---|---|---|
| `scene::Scene` | 现有改造 | 对象列表 + 脏标记；`collectRenderItems(camera, out)`（替代 `getDrawBatches`）；**零 Vulkan 类型** |
| `scene::SceneNode` | **新**（激活死代码 `Transform`） | 变换层级：局部/世界矩阵、parent/child |
| `scene::SceneObject` | 现有 `RenderObject` 拆分 | 纯数据：`MeshData` 句柄 + `MaterialInstance` 句柄 + 节点 + CPU 实例数组 + AABB |
| `scene::Camera` | 现有迁入 | **纯数学**（view/proj/position）；输入事件改由 Layer 5 的控制器驱动 |
| `scene::Frustum` / `SpatialIndex`（接口） | **新**（后续） | 视锥体 + 空间分区接口（BVH/均匀网格），为 README 中的 Frustum Culling 铺路 |

#### Layer 5 · Application（应用层）

| 模块 | 来源 | 职责 |
|---|---|---|
| `app::App` | 现有 `CEngine` 改造 | 组合根：创建各层子系统、装配、主循环（update → render） |
| `app::GameLoop` | **新** | deltaTime（迁自 Renderer::updateCamera 的计时逻辑）、固定步长 |
| `app::CameraController` | **新**（拆自 Camera + Renderer::updateCamera） | 读 `platform::Input`，驱动 `scene::Camera`（WASD/鼠标/滚轮） |
| `app::DemoScene` | 拆自 `CEngine::initScene` | 火星 + 1000 石头的内容脚本（含随机分布参数） |
| `app::Config` | 收编散落常量 | 窗口尺寸、验证层、扩展、资源根路径、MSAA（c_engine.hpp/context.hpp 的 inline 常量全部迁入） |

### 2.3 层间接口设计（关键契约）

```cpp
// ========== Layer 1 → 上层 ==========
namespace platform {
class Window {
public:
    static std::unique_ptr<Window> create(const WindowConfig&);
    void  pollEvents();
    void  waitEvents();                                  // swapchain 重建时等待
    bool  shouldClose() const;
    glm::uvec2 framebufferSize() const;
    std::vector<const char*> requiredInstanceExtensions() const;  // 替代 glfwGetRequiredInstanceExtensions
    vk::SurfaceKHR createSurface(vk::Instance) const;            // 替代 glfwCreateWindowSurface
    std::function<void(glm::uvec2)> onFramebufferResize;         // 替代 framebufferResized 标志
    // GLFWwindow* 只在此文件出现
};
enum class Key : uint32_t { W, A, S, D, Space, LeftShift, /* ... */ };
enum class MouseButton : uint8_t { Left, Right, Middle };
class Input {
public:
    void poll(const Window&);
    bool isKeyDown(Key) const;
    bool isMouseDown(MouseButton) const;
    glm::dvec2 cursorDelta();                            // 帧间增量，消费即清零
    double     scrollDelta();
};
}

// ========== Layer 2 → 上层 ==========
namespace resource {
using AssetId = uint64_t;
template <class T> struct AssetHandle { AssetId id; };

struct MeshData  { std::vector<Vertex> vertices; std::vector<uint32_t> indices; };  // CPU
struct ImageData { uint32_t w, h; std::vector<std::byte> pixels; };                  // CPU

class AssetLibrary {
public:
    AssetHandle<MeshData>  loadMesh(std::string_view path);
    AssetHandle<ImageData> loadImage(std::string_view path, const ImageImportOptions&);
    void unload(AssetId);            // 引用计数 -1
    void releaseUnused();            // 卸载引用计数归零的资产
};

using MeshGpuHandle    = uint32_t;   // 注册表索引（句柄化，替代裸 shared_ptr）
using TextureGpuHandle = uint32_t;

class ResourceRegistry {             // 全部 GPU 资源的所有者
public:
    MeshGpuHandle    createMesh(const MeshData&);
    TextureGpuHandle createTexture(const ImageData&, const TextureDesc&);
    const MeshGPU*   get(MeshGpuHandle) const;
    TextureGpuHandle defaultAlbedo() const;   // 内建默认资源
};
}

// ========== Layer 3 → 上层（Scene 消费的契约） ==========
namespace render {
struct RenderItem {                  // 纯数据，无 Vulkan/GLFW 类型
    resource::MeshGpuHandle    mesh;
    MaterialHandle             material;
    glm::mat4                  transform;
    uint32_t                   instanceOffset;   // 实例缓冲段
    uint32_t                   instanceCount;
};
class Renderer {
public:
    struct FrameContext { /* imageIndex、cmd buffer、UBO 映射指针、描述符集 */ };
    FrameContext beginFrame();                          // acquire + wait fence（out-of-date 内部重建）
    void record(const FrameContext&, std::span<const RenderItem>);
    void endFrame(const FrameContext&);                 // submit + present
};
}

// ========== Layer 4 → 上层 ==========
namespace scene {
class Scene {
public:
    void addObject(SceneObject&&);
    void collectRenderItems(const Camera&, std::vector<render::RenderItem>& out);  // 未来在此插入剔除
};
class Camera { /* 纯数学 */ };
}

// ========== Layer 5 ==========
namespace app {
class App { /* 组合根 + 主循环 */ };
struct Config { /* 窗口/路径/验证层/MSAA */ };
}
```

**每帧数据流（目标）**：

```
App::run
 ├─ GameLoop::tick()
 │    ├─ window.pollEvents(); input.poll(window); dt = clock.tick()
 │    ├─ cameraController.update(input, dt, camera)
 │    ├─ scene.collectRenderItems(camera, items)        // 场景层 → RenderItem
 │    ├─ renderer.beginFrame() → ctx
 │    ├─ renderer.record(ctx, items)                    // BatchBuilder 合并 → CommandRecorder 录制
 │    └─ renderer.endFrame(ctx)                         // submit + present
 └─ (resize 经 window.onFramebufferResize → renderer.onResize)
```

### 2.4 建议的目录结构（最终形态）

```
src/ （单树：每个 .hpp 与对应 .cpp 同目录，已无独立 include/ 树）
├── platform/   window.{hpp,cpp}  input.{hpp,cpp}  platform_utils.{hpp,cpp}
├── rhi/        vulkan_device.*  vma_allocator.*  command_pool.*  rhi_factory.*
│               debug_messenger.*  surface.*  swapchain.*  render_context.hpp  vertex_input.*
├── resource/   asset_library.*  mesh_importer.*  texture_importer.*  mesh_data.*
│               resource_registry.*  mesh_gpu.*  texture_gpu.*  upload_queue.*  buffer.hpp  sampler.*
├── render/     renderer.*  frame_resources.*  frame_uniforms.hpp  command_recorder.*
│               batch_builder.*  pipeline_cache.*  pipeline_spec.hpp  shader_manager.*
│               descriptor_manager.*  material.*  render_settings.*  render_item.hpp
├── scene/      scene.*  scene_node.*  scene_object.*  camera.*  transform.*  culling.*
├── app/        app.*  game_loop.*  camera_controller.*  demo_scene.*  config.*
└── main/       main.cpp
```

### 2.5 设计模式应用校验（Component / Service Locator）

> 校验依据（四份原文均已核对）：
> 1. 教程《Engine Architecture: Architectural Patterns》——`docs/Engine Architecture_ ...mhtml`（正文完整）；
> 2. 教程附录《Detailed Architectural Patterns》——`docs/Appendix_ __ Vulkan Documentation Project.mhtml`（含 Layered Architecture 原文，见 §2.7）；
> 3. GPP《Component》（`docs/Component · ...html`）；
> 4. GPP《Service Locator》（`docs/Service Locator · ...html`）。
>
> **关键确认（教程原文）**：主章节的分层定义与你的五层完全一致——"Typical layers include **platform abstraction, resource management, rendering, scene management, and application layers**"；附录进一步给出**逐字一致**的五层职责定义与示意图接口（§2.7）。教程的最终结论是**组合使用各模式**："component-based architecture provides the best foundation ... using **layered architecture** for our overall engine structure, **data-oriented design** for performance-critical systems, and **service locators** for cross-cutting concerns"。据此新增 §2.6（组件式架构落地）与 DOD 要求。

#### 2.5.1 Component 模式：怎么切、切完怎么通信

GPP 原文要点：**一个实体横跨多个领域时，按领域边界把代码切成独立组件，实体退化为"组件的薄容器"**；组件之间互不知晓；通信方式三选一（可混用）：① 修改容器共享状态、② 组件互持引用、③ 消息（中介者）。

| GPP 原文要点 | 本方案的落地 |
|---|---|
| 适用信号："A class is getting massive and hard to work with"、"touches multiple domains" | `Renderer`（380 行、7 类外部依赖）与 `CEngine`（17 个成员）正是该信号；Phase 3/5 按领域切片 |
| 按领域边界切片（原文：input/physics/graphics 各成组件） | `Renderer` → `FrameResources`/`CommandRecorder`/`BatchBuilder`/`PipelineCache`；`CEngine` → `App`/`GameLoop`/`CameraController`/`DemoScene` |
| 实体 = 薄容器 + 共享状态（原文：position/velocity 留在 Bjorn 里） | `SceneObject` = 薄容器（mesh 句柄 + material 句柄 + transform + 实例数组）；帧级共享状态集中在 `FrameContext` |
| 通信① 修改容器状态（"InputComponent 改 velocity，PhysicsComponent 后来读"） | `FrameContext`（imageIndex、命令缓冲、UBO 映射）就是"容器共享状态"：`BatchBuilder` 与 `CommandRecorder` 互不知晓，只读写 FrameContext |
| 通信② 互持引用（"simple and fast，但紧耦合"） | 仅限**同层内**（如 `Renderer` 直连 `Swapchain`）；跨层一律用接口契约（`RenderItem`/`AssetHandle`） |
| 通信③ 消息/中介者（"most complex"） | **本期不引入事件总线**——GPP 明确建议 "start simple and then add in additional communication paths if you need them"；`Window::onFramebufferResize` 回调即最小化的消息机制 |
| "Keep in mind：复杂度与过度设计风险" | 切片粒度以"类职责一句话能说清"为准，不为模式而模式；Phase 1 只切确定要切的 |
| 组件化 → Data Locality 收益（原文 flip side） | 场景层实例数据现为 AoS；未来做剔除时可按 SoA 整理（`SpatialIndex` 预留落点）；热路径（绘制循环）保持 `RenderItem` 扁平 POD，避免组件指针跳转 |

补充 GPP 对"谁提供组件"的讨论：容器自己创建（保证不缺件、难重配）vs 外部注入（灵活、可换实现）。本方案取**外部注入**：`App`（组合根）负责装配各层组件，层与层只见接口——这正是 Phase 5 把 `CEngine` 改成组合根的依据。

#### 2.5.2 Service Locator：哪些服务该全局、哪些该注入

GPP 原文要点：服务定位器 = **抽象服务接口 + 具体提供者 + 定位器**；"**sparingly（慎用）**——先考虑显式传参，传参确实啰嗦或服务本质单一（日志、内存、音频）才用定位器"；用 **Null Object** 提供者消除 temporal coupling；用 **Decorator** 提供者做开发期日志。

| 服务 | 现状 | 目标方案 | 理由 |
|---|---|---|---|
| RHI 工厂（原 `ResourceFactory`） | 裸单例、无接口 | **构造器注入**：`RhiFactory` 由 rhi 上下文持有，显式传给 resource/render 层 | GPP："first consider passing the object to it instead"；它是核心依赖，显式传参让依赖一目了然 |
| 日志 | `std::cout/cerr` 散落 4+ 文件 | **Service Locator**：`Log` 接口 + `ConsoleLog`/`NullLog` + `LogLocator::provide/get` | 日志是"本质单一的环境性设施"，逐层传参确实啰嗦——正符合 GPP 的使用场景 |
| 时间/帧计时 | `Renderer::updateCamera` 内静态时钟 | 注入（`GameLoop` 持有，传给需要者） | 使用方少，显式传参即可 |
| 默认资源（白贴图/平坦法线） | 主程序手工创建 + 手工兜底（engine.cpp:126-129） | **Null Object 语义正式化**：`AssetLibrary::find` 未命中 → 空句柄 → `ResourceRegistry` 回落默认资源 | 现状已是雏形；GPP Null Object 章节正是"找不到也保证返回可用对象" |
| 可选装饰器 | — | 开发期 `LoggedAssetLibrary`/`LoggedRhiFactory`（包装 + 转发） | GPP Decorator 示例（LoggedAudio）；需要时再加，不进主线 |
| 作用域 | — | `Log` 全局；其余服务一律限制在所属层内（构造器传递） | GPP："service restricted to a single domain → limit its scope to a class" |

> 对分层架构的补充理解：定位器本身属于 **Layer 1（platform）**，只承载 `Log` 这类跨层服务；它**不能**成为跨层传递 GPU 对象的通道——层间仍走显式接口（`RenderItem`/`AssetHandle`），避免"全局可达 = 全局耦合"（这正是教程 Layered Architecture 与 GPP 对全局访问的共识）。

#### 2.6 组件式架构（Entity / Component / System）——教程重点

教程以组件式架构为引擎基础（"Component-based architecture ... forms the foundation of our Vulkan rendering engine"），参考实现是：`Entity` 持有组件容器（`AddComponent<T>` / `GetComponent<T>`），`TransformComponent`、`MeshComponent` 挂接在实体上，`System` 处理特定组件。教程列举的理由：图形特性灵活增删（换着色模型/后处理/光照无需大重构）、渲染关注点分离（几何/材质/光照/相机各自成件）、行业标准、易于纳入新 Vulkan 特性、与数据导向优化兼容。

对照本项目：

| 教程概念 | 本方案对应 | 落地说明 |
|---|---|---|
| Entity（实体 = 容器） | `scene::SceneObject` | 保留"薄容器"形态（句柄 + transform + 实例数组），不做动态组件增删 |
| TransformComponent | `SceneNode` / `Transform`（激活现有死代码） | 教程示例恰好就是 TransformComponent（position/rotation/scale） |
| MeshComponent | mesh 句柄 + material 句柄成员 | 教程示例中 MeshComponent 持有 Mesh*/Material* —— 本方案改为**句柄**，所有权归 `ResourceRegistry` |
| System（处理特定组件） | `render::BatchBuilder`（批合并）、`CommandRecorder`（录制）、未来 `CullingSystem` | System = 处理组件集合的纯逻辑，不持有场景数据 |
| 灵活换渲染特性 | Phase 3 的 `PipelineSpec`/`PipelineCache` + 独立 pass | 换 shading model = 换 spec，场景/应用层零改动 |
| 数据导向（DOD） | 见下 | 性能关键路径 SoA + 扁平 POD |

**为什么不用教程示例那种动态 `GetComponent<T>`（dynamic_cast 版本）**：教程示例是教学演示；GPP 明确警告组件化的"间接跳转成本"与"过度设计风险"。本项目组件数量少且固定（transform/mesh/material），用**固定成员（typed component bag）**即可获得同样的关注点分离，避免 RTTI 与堆分配；若未来场景对象种类爆炸（粒子、光源、触发器），再升级为真正的 ECS（实体 = ID + 并行 System），`SceneObject` 的接口边界已为此预留。

**DOD（Data-Oriented Design）落点**（教程："data-oriented design for performance-critical systems"）：
- `RenderItem` 保持扁平 POD（Phase 3），绘制循环零虚调用、零指针跳转；
- 实例数据 SoA 化（Phase 4 预留）：未来按"所有对象的 transform 连续存放"整理，配合 `SpatialIndex` 做剔除时一次遍历一片内存；
- 批合并（`BatchBuilder`）保证同 (mesh, material) 连续绘制，最大化顶点缓冲局部性。

#### 2.7 附录《Layered Architecture》校验（最后一块拼图）

附录的"Layered Architecture"章节与主章节互补，五层职责定义与你最初的需求描述**逐字一致**：

> - **Platform Abstraction Layer** - Provides a consistent interface to platform-specific functionality.
> - **Resource Management Layer** - Manages loading, caching, and unloading of assets.
> - **Rendering Layer** - Handles the rendering pipeline, shaders, and graphics API interaction.
> - **Scene Management Layer** - Manages the scene graph, spatial partitioning, and culling.
> - **Application Layer** - Handles user input, game logic, and high-level application flow.

**附录的示意图接口**与我们的对应：

| 附录接口（原文示例） | 本方案 | 差异说明 |
|---|---|---|
| `Platform::Initialize / CreateWindow / ProcessEvents` | `platform::Window`/`Input`/`PlatformUtils` | 一致；`void* CreateWindow` 的 C 风格由 RAII 类型替代 |
| `ResourceManager::LoadTexture/LoadMesh(path)` | `resource::AssetLibrary`（`AssetHandle`） | 一致；裸指针返回改为句柄 |
| `Renderer::Initialize(Platform*) / RenderScene(Scene*)` | `render::Renderer` + `RenderItem` 契约 | **一处张力，见下** |
| `SceneManager::AddEntity / UpdateScene(dt)` | `scene::Scene` + `app::GameLoop` | 一致 |
| `Application::Run()` 主循环（ProcessEvents → UpdateScene → RenderScene） | `app::App::run()`（§2.3 目标数据流） | 一致 |

**一处张力与取舍**：附录示例 `Renderer::RenderScene(Scene* scene)` 意味着渲染层引用了场景类型（渲染层在上、场景层在下）。而你的分层顺序是 Scene(4) 在 Rendering(3) 之上，依赖只能向下。调和方式：**`RenderItem` 契约**——场景层把可见对象折叠成纯数据 `RenderItem`（render 层定义），`Renderer::record(ctx, span<const RenderItem>)` 不再认识 `Scene`。这样既保住你的层序，又比附录示例更符合"可独立替换各层"的收益描述；附录示例本身只是示意接口，未展开依赖分析，不构成冲突。

**附录的比较分析表**（Layered vs Component vs DOD vs Service Locator）对我们的选型影响：

| 附录结论 | 本方案响应 |
|---|---|
| Layered 弱点："layer bloat"、"unnecessary indirection"、"potential performance overhead from layer traversal" | 新增风险 R10；帧内热路径不层层转发（RenderItem 直通） |
| Component-Based："more complex initially ... harder to debug" | 坚持 typed component bag，不上完整 ECS（附录结论："A small project may not need the complexity of a full ECS"） |
| DOD："less intuitive ... steeper learning curve" | 只用于性能关键路径（RenderItem/实例数据 SoA），不扩散到全部代码 |
| Service Locator："can hide dependencies / global state concerns" | 仅 `Log` 一个定位器，其余构造器注入（§2.5.2） |
| 结论："most engines use a combination of these patterns" | 与本方案"分层骨架 + 组件实体 + DOD 热路径 + 定位器横切"的组合一致 |

**附录 Advanced Rendering Techniques 的提示**：Deferred（Geometry Pass + Lighting Pass）与 Forward+（Light Culling Pass）说明渲染层必须**按 pass 组织**——Phase 3 的 `CommandRecorder`/`PipelineSpec`/`PipelineCache` 正是为此预留；未来 path tracing pass 同理（README 的最终目标）。

---

## 3. 现状 → 目标差距映射

| 现有类 | 目标层 | 需要的改动 |
|---|---|---|
| `CEngine` | app（App） | 拆分：窗口/输入/内容/配置/循环全部外移，只剩装配 |
| `VulkanDevice` | rhi | 去 GLFW（扩展来源改为 Window） |
| `Context` | rhi | 拆为 `DebugMessenger` + `Surface` |
| `ResourceFactory`（单例） | rhi（RhiFactory） | 去单例化；合并重复的转换/格式查询 |
| `Swapchain` | rhi | 依赖 `Window&`；显式清理 color/depth |
| `Renderer` | render | 拆出 FrameResources/CommandRecorder；输入、灯光、相机外移 |
| `Pipeline` | render | 参数化（PipelineSpec）+ 缓存；ShaderManager 接管读文件/反射 |
| `DescriptorSetLayout/...` | render | 整合为 DescriptorManager；删死代码 |
| `AssetManager` | resource | 句柄化 + 引用计数 + 卸载 + 路径解析 |
| `Mesh` / `Texture` | resource+render | CPU 半（MeshData/ImageData+Importer）与 GPU 半（MeshGPU/TextureGPU）分离 |
| `Material` | render | 引用 TextureGpuHandle；描述符集不变 |
| `Buffer<T>` / `Sampler` | resource | 迁入；走 UploadQueue |
| `Scene` / `RenderObject` | scene | 纯数据化；DrawBatch → RenderItem（定义在 render 层） |
| `Camera` | scene | 纯数学；输入由 App 层驱动 |
| `Transform` | scene（SceneNode） | 从死代码激活 |
| `glTFModel` | resource（MeshImporter） | 接入资产库 |
| `Vertex`/`InstanceData` | rhi | 迁入（顶点输入描述） |
| 散落常量（路径/尺寸/扩展） | app（Config）+ platform | 收编 |
| `MAX_FRAMES_IN_FLIGHT` | render（FrameResources） | 迁入 |
| `generic/` 目录 | — | 解散，按 2.4 分流 |

---

## 4. 分阶段重构步骤

> 通用纪律：每阶段结束 = 能编译 + 能运行 + 画面与阶段开始前一致（建议先截一张"黄金帧"对照）；每阶段一个 git 提交；Debug 构建保持 validation layers 打开。

### Phase 0 · 基线固化（0.5 天）

1. 提交当前未提交的 glTF 改动与新增资源（`git status` 里的 `glFTloader`、`bedroom.obj`、`iscv2_*` 贴图），打 tag `pre-refactor`；
2. Debug/Release 各构建一次，确认无警告/无 validation 报错；
3. 保存黄金帧截图 + 运行日志；
4. 顺手记录当前帧率（后续对比性能）。

**验收**：`git status` 干净；双配置构建通过。

### Phase 1 · 平台抽象层落地（1.5~2 天）

目标：GLFW 只出现在 `platform/`；渲染/设备/场景层不再 include GLFW。

1. 新建 `src/platform/window.{hpp,cpp}`：把 `CEngine::initWindow`、4 个静态回调、`glfwFramebufferResizeCallback` 迁入；`Window` 暴露 `requiredInstanceExtensions()`、`createSurface()`、`onFramebufferResize` 回调（替代 `framebufferResized` 标志）；`CEngine` 的 `GLFWwindow* window` 成员替换为 `std::unique_ptr<Window>`；
2. 新建 `src/platform/input.{hpp,cpp}`：`Key`/`MouseButton` 枚举 + 状态查询；把 `Renderer::updateCamera` 里的 `glfwGetKey` 轮询整体迁出；
3. 改 `VulkanDevice`：`GetRequiredExtension()` 改为接收 `window->requiredInstanceExtensions()`；
4. 改 `Context`：`createSurface` 改用 `window->createSurface(*instance)`；随后把 `Context` 拆为 `DebugMessenger` + `Surface` 两个小类（或保留一个 `RhiBootstrap`）；
5. 改 `Swapchain`：构造与 `recreateSwapChain` 改收 `Window&`，内部 `glfwGetFramebufferSize` 换成 `window->framebufferSize()`；
6. 改 `Camera`：`onMouseButton(int,...)` 参数改为 `MouseButton`；`camera.cpp` 里的 `GLFW_MOUSE_BUTTON_LEFT` 换成枚举；`camera.hpp` 去掉 GLFW include；
7. `CEngine::run()` 改为 `window->pollEvents()` / `window->shouldClose()`；
8. 新建 `platform::Log` 服务定位器（`Log` 接口 + `ConsoleLog`/`NullLog` 提供者 + `LogLocator::provide/get`），替换 vulkandevice.cpp:87、asset_manager.cpp:20、renderer.cpp:150-156、context.cpp:26 的散落输出——Service Locator 模式的第一个落地样例（见 §2.5.2）。

**验证**：全局 grep 确认 `glfw` 只出现在 `platform/`（与 `extern/`）；运行：窗口、鼠标轨道、WASD、滚轮、resize（含最小化恢复）全部正常；`LogLocator::provide(nullptr)` 回落 NullLog 不崩溃。

### Phase 2 · 资源管理层（2~3 天）

目标：CPU 资产与 GPU 资源分离；句柄化；路径解析。

1. 新建 `platform::PlatformUtils`：`assetRoot()`；`app::Config` 里放资源根路径；把 `context.hpp` 的 `../models/...` 常量收编进 Config；
2. 新建 `resource::MeshData`/`ImageData`；把 `Mesh` 构造器里的去重/法线/切线生成逻辑迁到 `MeshData::postProcess()`；新建 `MeshImporter`（OBJ + 把 `glTFModel` 的解析逻辑并入，注册为 glTF 导入器）；
3. 新建 `resource::TextureImporter`（stb 加载 → ImageData）；`Texture::createTexture` 拆为 importer + GPU 上传；
4. 新建 `resource::ResourceRegistry` + `MeshGPU`/`TextureGPU`（Mesh 的缓冲半、Texture 的图像/视图半迁入）；句柄替代 `shared_ptr` 传递；
5. 新建 `resource::UploadQueue`（封装 transient pool 的单次提交 + staging 模板），`Buffer<T>` 与注册表改走它；
6. `AssetLibrary` 替换 `AssetManager`：`loadMesh/loadImage` 返回 `AssetHandle`，内部引用计数；默认纹理成为注册表内建资源（`engine.cpp` 的手工创建删除），并正式化 **Null Object 语义**：`find` 未命中返回空句柄，渲染层回落默认资源（GPP Service Locator 章节的 Null 服务思想，§2.5.2）；
7. `CEngine::initAssetManager` 改为 `AssetLibrary` 装配（阶段内保留在 App 雏形中）。

**验证**：画面一致；重复 `loadMesh(同一路径)` 只上传一次（可在注册表加日志验证）；`../` 相对路径从代码中消失（grep）。

### Phase 3 · 渲染层拆分（2~3 天）

目标：Renderer 只剩帧编排；管线/描述符/录制各归其位。

1. 清理：删 `instanceText`、`DescriptorSet` 类；合并 `findSupportedFormat` 与布局转换到 `rhi::RhiFactory`（`ResourceFactory` 去单例化，由 rhi 上下文持有并注入）；
2. 新建 `render::FrameUniforms`（UBO/Light 迁出 renderer.hpp）；`MAX_FRAMES_IN_FLIGHT` 迁到 `render::FrameResources`；
3. 从 Renderer 拆出 `FrameResources`（sync 对象、命令缓冲、UBO 缓冲、per-frame 描述符集）；Renderer 只留 `beginFrame/record/endFrame` 编排逻辑；
4. 新建 `render::PipelineSpec`（管线状态 POD）+ `PipelineCache`；`Pipeline` 改造为按 spec 创建；shader 路径改从 Config 取；
5. 新建 `render::ShaderManager`：SPIR-V 读取 + 反射（接管 `DescriptorSetLayout` 的 `autoCreateDSL` 输入侧）；
6. 新建 `render::CommandRecorder`：`recordCommandBuffer` 的录制逻辑整体迁入，输入改为 `std::span<const RenderItem>`；
7. `Renderer::updateCamera` 删除（计时与输入迁到 Layer 5，本阶段可先由 App 循环调用临时接口过渡）；灯光常量迁到 `DemoScene`/Config；
8. 新建 `render::RenderSettings`：MSAA（替代硬编码 4x）、present mode、分辨率。

**验证**：帧同步逻辑（timeline + binary semaphore + fence）保持原样，仅搬位置；连续运行 10 分钟无 validation 报错；resize 正常。

### Phase 4 · 场景层纯数据化（2 天）

目标：场景层零 Vulkan/GLFW 类型；为剔除铺路。

1. 在 render 层定义 `RenderItem`（含 `MeshGpuHandle`/`MaterialHandle`/transform/实例段）；
2. `Scene::getDrawBatches` 删除；`Scene::collectRenderItems(camera, out)` 产出 RenderItem 向量；`DrawBatch` 迁到 render 层由 `BatchBuilder` 消费（合并逻辑保留，dirty 缓存保留）；
3. `RenderObject` 拆为 `scene::SceneObject`（CPU：mesh 句柄、material 句柄、实例数组、AABB）+ GPU 实例缓冲（由 render 侧 `InstanceBufferManager` 或 resource 层按需创建/更新）；
4. 激活 `Transform`：新建 `SceneNode`（父/子、世界矩阵）；`SceneObject` 挂节点；实例矩阵由 transform 合成（现阶段保持每对象独立实例缓冲，行为不变）；
5. `Camera` 迁入 scene 命名空间，删除输入相关成员（只留数学）；
6. ~~预留 `Frustum`/`SpatialIndex` 接口与 `SceneView` 结构~~（**已取消**：项目最终目标为纯路径追踪离线渲染器，光栅剔除不再适用；见 §6 路线图）。

**验证**：grep 确认 `scene/` 头文件无 `vulkan`/`GLFW` include；实例绘制数量不变（1001 个对象、1000+1 实例）。

**实施记录（2026-08-28）**：
- `Scene::collectRenderItems()` 直产 `std::span<const RenderItem>`——因剔除取消，签名不带 camera 参数；dirty 缓存保留，帧间零分配；
- RenderItem 无 Vulkan 类型：实例段改为 `render::InstanceBuffer` 句柄（每对象 GPU 实例流，UploadQueue 一次性上传），`DrawBatch` 删除；
- `AssetHandle` 拆出独立 CPU 侧头 `resource/asset_handle.hpp`；`Vertex/InstanceData` 纯数据化（Vulkan 输入布局描述移入 `pipeline.cpp`）；
- 材质 Set-1 描述符改由引擎按材质分配一次（原 RenderObject 逐对象惰性分配，共享材质本就复用同一 set，行为不变）；
- `UploadQueue` 自持 allocator，scene 层 API 不再出现 `VmaAllocator`；
- AABB 未引入（随剔除一并取消）；`SceneNode` 父子层次暂缓——层次结构届时由 TLAS/BLAS 场景装配承接，`worldMatrix()`（dirty-flag 惰性缓存）已就位。

### Phase 5 · 应用层成形（1.5 天）

1. `CEngine` 改名/重构为 `app::App`：成员收敛为各层子系统指针 + Config；`initVulkan` 的 10 步装配按层归位（rhi 装配 → 资源 → 渲染 → 场景 → 内容）；
2. 新建 `app::GameLoop`：deltaTime 计时（迁自 Renderer::updateCamera 的 `high_resolution_clock` 逻辑）+ 固定步长骨架；
3. 新建 `app::CameraController`：`update(input, dt, camera)` 实现 WASD/鼠标/滚轮；
4. 新建 `app::DemoScene`：`CEngine::initScene` 的内容（火星 + 1000 石头分布）整体迁入，参数集中；
5. 新建 `app::Config`：收编 `WIDTH/HEIGHT`、验证层、扩展、路径、MSAA。

**验证**：`c_engine.hpp` 消失；`app::App` 的 `run()` 只做装配 + 循环转发；行为与 Phase 4 完全一致。

**实施记录（2026-08-28）**：
- 五项全部落地：`app::App`（装配按层归位：initWindow → initRhi → initResources → initSamplers → initContent → initRender）、`app::GameLoop`（deltaTime 计时 + 固定步长骨架预留）、`app::CameraController`（左键拖拽轨道状态机 + WASD/Space/Shift/滚轮，参数原值迁移）、`app::DemoScene`（三材质 + 火星 + 1000 岩石；灯光/FOV/远近平面收编为 `render::FrameParams`，经 `Renderer::Dependencies` 下发——渲染层依旧不依赖内容层）、`app::Config`（窗口、验证层与设备扩展、MSAA、present mode、路径全部收编）；
- 材质 Set-1 描述符改由 `App::initRender` 经 `DemoScene::materials()` 按材质分配一次；
- 附带工程修复：CMake POST_BUILD 将 ucrt64 运行时 DLL（libstdc++/libgcc/libwinpthread/glfw3）落地 `bin/`，消除 PATH 上 mingw64（MSVCRT 版）运行库被优先解析导致的 `STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139)` 启动崩溃；bin/ 自包含、可双击运行；
- 新增仓库根 `AGENTS.md`：面向代理/新协作者的构建运行指南、五层架构与模块地图、跨层契约、工程约定。

### Phase 6 · 构建层与工程纪律（1 天）

1. CMake 拆分 5 个静态库目标（platform → resource → render → scene → app），`main` 只链 `app`：
   ```cmake
   add_library(platform STATIC ...)   # 链 Vulkan::Vulkan glfw KTX
   add_library(resource STATIC ...)   # PRIVATE platform
   add_library(render   STATIC ...)   # PRIVATE resource
   add_library(scene    STATIC ...)   # PRIVATE render
   add_library(app      STATIC ...)   # PRIVATE scene
   add_executable(main main.cpp)      # PRIVATE app
   ```
   → 链接方向即依赖方向，出现反向 include 直接编译失败；
2. `GLOB_RECURSE` 换显式源文件列表（分层后每库文件不多，顺带获得干净的增量构建）；
3. 命名空间落地：`platform::/rhi::/resource::/render::/scene::/app::`（可与 Phase 1-5 同步进行，最后统一收尾）；
4. 头文件卫生：删 `#include "c_engine.hpp"` 之类的违规包含（context.cpp/camera.cpp）；统一 `VULKAN_HPP_*` 宏只在一处定义；
5. 可选：clang-format 配置、CI（双配置构建 + validation 运行）、数学单元测试（Transform/Camera/Frustum）。

**验收**：`cmake --build` 全新构建通过；`#include <vulkan>` 反向入侵（scene/app 头文件）在编译期被拒绝。

---

## 5. 风险与最佳实践

### 5.1 风险清单

| # | 风险 | 等级 | 缓解措施 |
|---|---|---|---|
| R1 | **大爆炸重构**：一次改完再编译 | 高 | 严格按 Phase 0-6 渐进；每阶段独立提交、可运行；每阶段开头先跑黄金帧对比 |
| R2 | **资源生命周期错乱**（描述符池先于 set 销毁、swapchain 重建时句柄失效） | 高 | 保持现有"池先于 set 声明"的成员顺序纪律（c_engine.hpp:51-54 注释）；句柄化后用"注册表持有、层内引用"替代跨层裸指针；swapchain 重建路径回归测试（反复拖拽窗口 + 最小化） |
| R3 | **同步逻辑被"顺手优化"改坏** | 高 | Phase 3 只搬代码不改语义；timeline semaphore 流程原样保留；每阶段跑 validation layers + 长时间运行 |
| R4 | **单例去除引发的初始化顺序问题** | 中 | `ResourceFactory` 改为显式创建的 `RhiFactory` 对象，构造顺序由 App 装配明确控制（消除"谁先 init"的隐式依赖） |
| R5 | **CWD 路径问题**：`../shaders/slang.spv` 依赖启动目录 | 中 | Phase 2 就引入 `assetRoot()` 解析；slang 编译输出路径同步改由 CMake 传给程序（如编译期宏 `SHADER_DIR`） |
| R6 | **性能回归**：批合并、实例化、缓存策略被破坏 | 中 | BatchBuilder 保留 dirty 缓存与合并逻辑；每阶段对比帧率与 draw call 数（可用 validation 的 performance 消息观察） |
| R7 | **RenderItem 拷贝开销** | 低 | `collectRenderItems` 输出到复用 vector（out 参数）；RenderItem 保持 POD；避免每帧分配 |
| R8 | **CMake 拆分后 glTF/外部库链接漂移** | 低 | 外部库（KTX/tinygltf3/spirv_reflect）挂在 platform 或独立 `extern` 目标；`_DEBUG`/`TINYGLTF3_ENABLE_FS` 的 per-file 设置（CMakeLists.txt:65-68）随文件迁移时一并带上 |
| R9 | **工作区已有未提交改动**（glTF 加载器） | 低 | Phase 0 先提交基线，避免重构与功能开发互相污染 |
| R10 | **层膨胀与层间间接跳转**（附录原文点名的 Layered 弱点："layer bloat"、"unnecessary indirection"、"potential performance overhead from layer traversal"） | 中 | 分层只约束**头文件依赖方向**，不约束帧内数据流：`RenderItem` 扁平 POD 直通录制循环，不搞"每帧层层转发"；层内组件直连（同层允许互持引用）；若某层接口出现"为过层而过层"的空转发，合并该接口 |

### 5.2 最佳实践（对应教程设计原则）

1. **清晰的责任分离**（教程 Architectural Patterns 的核心）：每个类只回答一个问题——`Renderer` 回答"帧怎么编排"，`CommandRecorder` 回答"命令怎么录"，`Scene` 回答"场景里有什么"；判断标准：给类写一句话职责描述，写不出来就是职责过多。
2. **依赖方向可执行**：光靠自觉不够——用 CMake 库目标把"禁止向上依赖"变成编译错误；这是教程 Layered Architecture 落地的关键工程手段。
3. **接口最小化**：层间只暴露必要接口（`RenderItem`/`AssetHandle`/`FrameContext`）；`vk::`/`GLFW` 类型不出层，句柄出层。
4. **句柄优于裸指针/引用**：`shared_ptr` 跨层传递会形成隐式生命周期契约；`AssetHandle`/`MeshGpuHandle` 让所有权归属明确（注册表），也便于未来做流式加载与热重载。
5. **RAII 纪律保持**：项目现有的 move-only 语义（`VmaBuffer`/`VmaImage`/`Texture`/`Material`）很好，重构中延续；新增资源类一律禁止拷贝。
6. **数据与行为分离**：MeshData（数据）+ MeshImporter（解析）+ MeshGPU（GPU 表现）三段式，是后续多线程导入（解析在 worker 线程、上传走 transfer queue）的前提。
7. **配置外置**：尺寸/MSAA/路径/灯光这类"内容"从代码里拿出来（`Config`/`DemoScene`），学习期改参数不再需要重新编译。
8. **为 path tracing 留门**：目标渲染器最终要加 path tracing pass——渲染层拆出 `CommandRecorder`/`PipelineCache` 后，新增 compute/raytracing pass 只需在 render 层加 pass 组件，scene/app 层零改动。
9. **验证手段前置**：每阶段结束的验收标准（编译/运行/黄金帧/validation 干净）写死在 Phase 描述里；建议顺手在 README 勾掉"Frustum Culling"待办时补上剔除的验收场景。
10. **组件化（Component 模式）**：一个类若横跨多个领域（现在的 `Renderer`/`CEngine` 正是），按领域边界切片；切片后组件间通信**从简开始**——优先"容器共享状态"（`FrameContext`），必要时同层直连，最后才考虑消息机制；避免过早引入事件总线（GPP 原话："start simple and then add in additional communication paths if you need them"）。
11. **服务定位器慎用（Service Locator）**：默认构造器注入（依赖一目了然）；仅对"本质单一 + 全局需要"的服务（日志）使用定位器；定位器必须配 Null Object 提供者（`provide(nullptr)` 回落），杜绝 temporal coupling；服务接口与具体实现分离，开发期可套 Decorator 提供者加日志。
12. **组件式架构 + DOD（教程重点）**：实体 = 固定组件包（transform/mesh/material 成员），System = 处理组件集合的纯逻辑（`BatchBuilder`/`CommandRecorder`/未来 `CullingSystem`）；组件数量少时用 typed member 而非动态 `AddComponent`（避免 dynamic_cast 与堆分配）；性能关键路径坚持数据导向（扁平 POD、SoA、批量处理），与"分层做骨架、组件做实体、定位器做横切"的教程组合结论保持一致。

### 5.3 与官方教程章节的对应

| 教程章节 | 对应本方案 |
|---|---|
| [Engine Architecture: Introduction](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/01_introduction.html) | 引擎整体组织方式（第 2 节五层总览） |
| [Architectural Patterns](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/02_architectural_patterns.html) | 五层定义原文确认（§2.5 校验头注）；分层依赖规则 R1-R5（Phase 1/6）；组件式架构 + DOD + 服务定位器组合（§2.6） |
| [Component Systems](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/03_component_systems.html) | 子系统化：FrameResources/PipelineCache/DescriptorManager 等组件由引擎装配（Phase 3、5） |
| [Rendering Pipeline](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/05_rendering_pipeline.html) | PipelineSpec 参数化 + PipelineCache（Phase 3） |
| [Appendix: Detailed Architectural Patterns](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Appendix/appendix.html) | 五层职责逐字确认；示意图接口与 RenderItem 取舍（§2.7）；四种模式比较分析（§2.7） |
| [GPP: Component](https://gameprogrammingpatterns.com/component.html)（你提供的 docs 存档） | 上帝类切分与组件通信方式（§2.5.1；Phase 3/5） |
| [GPP: Service Locator](https://gameprogrammingpatterns.com/service-locator.html)（你提供的 docs 存档） | 单例改造决策、Null Object、日志服务（§2.5.2；Phase 1/2/6） |

---

## 6. 重构后路线：光线追踪管线（Path Tracing，NVIDIA GPU）

> 分层重构（Phase 0–6）完成后启动。目标形态：**纯路径追踪的离线渲染器**。
> 运行硬件为 NVIDIA GPU（完整支持 Vulkan Ray Tracing 全套扩展）。

1. **设备能力**：`rhi::VulkanDevice` 增查并启用 `VK_KHR_acceleration_structure`、`VK_KHR_ray_tracing_pipeline`、`VK_KHR_deferred_host_operations`（备选 `VK_KHR_ray_query`）；extension/feature 收进 `CreateInfo` 与 `Config`。
2. **加速结构**：新增 `rhi::AccelerationStructure`（BLAS/TLAS 构建）。BLAS 几何直接引用 `resource::MeshGPU` 的顶点/索引缓冲；TLAS instance 的 transform 来自 `scene::SceneObject::worldMatrix()`——Phase 4 激活的 Transform/实例数据即为此铺路。
3. **管线**：在 `GraphicsPipelineSpec` 之外新增 `RayTracingPipelineSpec`（raygen/closest-hit/miss 组 + SBT 布局），`PipelineCache` 按 spec 分派；`ShaderManager` 增加 RT SPIR-V 通道（slang 原生支持 ray tracing）。
4. **录制**：新增 trace pass 组件（`cmd.traceRays`），复用现有帧编排（`FrameResources` 同步骨架、`Renderer::beginFrame/record/endFrame` 三段式不变）。
5. **场景数据**：`Scene::collectRenderItems` 的数据面（mesh/材质/实例变换）即 TLAS instance 来源；材质由 GPU-backed `Material` 数据化为 BSDF 参数表（`generic/` 剩余件的最终归宿）。
6. **离线渲染**：`app::GameLoop` 旁增加离线帧模式（固定相机、累积采样、PNG/EXR 输出），不依赖窗口循环——这也是"离线渲染器"目标的交付形态。

---

## 7. 参考资料

- Vulkan 官方教程（docs.vulkan.org）《Building a Simple Engine》：
  - [Introduction](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/01_introduction.html)
  - [Architectural Patterns](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/02_architectural_patterns.html)
  - [Component Systems](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/03_component_systems.html)
  - [Rendering Pipeline](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/05_rendering_pipeline.html)
  - [Conclusion](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/conclusion.html)
  - [Appendix](https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Appendix/appendix.html)
- Game Programming Patterns（Robert Nystrom，你提供的本地存档）：
  - [Component · Decoupling Patterns](https://gameprogrammingpatterns.com/component.html)（对应 `docs/Component · Decoupling Patterns · Game Programming Patterns.html`）
  - [Service Locator · Decoupling Patterns](https://gameprogrammingpatterns.com/service-locator.html)（对应 `docs/Service Locator · Decoupling Patterns · Game Programming Patterns.html`）
  - ✅ 教程原文已核对：`docs/Engine Architecture_ Architectural Patterns __ Vulkan Documentation Project.mhtml`（浏览器另存的 MHTML 完整存档，正文含五层定义与组件式架构示例代码）
  - ✅ 附录原文已核对：`docs/Appendix_ __ Vulkan Documentation Project.mhtml`（MHTML 完整存档，含 Layered Architecture / DOD / Service Locator / 四种模式比较分析 / Advanced Rendering Techniques）
- 本仓库现状：`include/`、`src/`、`CMakeLists.txt`、`README.md`（2025 年基线）
