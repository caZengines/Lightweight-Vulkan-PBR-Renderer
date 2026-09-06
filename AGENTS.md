# AGENTS.md — Vulkan 路径追踪渲染器 · 项目架构与模块指南

> 面向在本仓库工作的 AI 代理与新协作者。细节文档：`docs/architecture-refactor-plan.md`（重构计划 + RT 路线图）、`docs/architecture-current.md`（当前架构全量说明）、`docs/input-action-layer-plan.md`（输入语义动作层重构 · 已批准，§4 相关行按该目标态描述）。

## 1. 项目是什么

- 学习型 Vulkan PBR 渲染器，**最终目标是纯路径追踪的离线渲染器**（运行硬件为 NVIDIA GPU）。
- 分层重构（Phase 0–5）已完成；重构全部结束后，光栅管线将按计划文档 §6 迁移到 Vulkan 光线追踪（`VK_KHR_acceleration_structure` / `VK_KHR_ray_tracing_pipeline`）。
- 技术栈：**C++20**（遵循 C++ Core Guidelines）、Vulkan SDK 1.4.341.1、vulkan-hpp RAII、VMA、slang（编译到 SPIR-V）、GLFW、glm。
- 编程风格硬性约定见 §6。

## 2. 构建与运行

```bash
# 配置 + 构建（Ninja；两个构建树可并存）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# 运行：cwd 必须是 bin/（platform::PlatformUtils 按 cwd 解析资产根）
cd bin && ./main.exe
```

- 验证层：Debug 开 / Release 关（`app::Config::enableValidationLayers`，由 `NDEBUG` 推导）。
- shader：`shaders/shader.slang` 由 CMake 自定义命令经 `slangc` 编译为 `shaders/slang.spv`，随构建执行。
- **bin/ 是自包含的**：CMake POST_BUILD 会把 ucrt64 工具链的 `libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll / glfw3.dll` 拷到 exe 旁边。背景：PATH 里 mingw64/bin 排在 ucrt64/bin 之前，若不落地本地 DLL，加载器可能解析到 MSVCRT 版运行库导致 `STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139)` 启动即崩。
- `bin/main.exe` 若有残留进程未退出，链接会失败——先结束进程再构建。
- 修改 `src/` 下任何文件名/目录（新增、移动、删除）后**必须重新 cmake 配置**再构建：源列表来自 `GLOB_RECURSE`（Phase 6 计划换成显式列表）。

## 3. 架构总览

五层分层（依 Vulkan 官方教程 Engine Architecture），**依赖只允许向下**：

```
Layer 5  Application  — src/app/      组合根、主循环、输入映射、示例内容     (app::)
Layer 4  Scene        — src/scene/    纯数据场景 + 相机数学（零 Vulkan/GLFW）(scene::)
Layer 3  Rendering    — src/render/   帧编排、管线、录制、每帧资源           (render::)
Layer 2  Resource     — src/resource/ 资产导入、GPU 资源注册表、上传队列     (resource::)
Layer 0/1 RHI         — src/rhi/      设备/交换链/VMA/屏障/表面等 Vulkan 封装 (rhi::)
Layer 1  Platform     — src/platform/ 窗口/输入/日志/路径（GLFW 唯一出入处）  (platform::)
```

- 构建系统当前是单可执行目标 + `GLOB_RECURSE`，分层暂靠纪律维持；Phase 6 将拆成 5 个静态库，用 `target_link_libraries` 把依赖方向变成编译期约束。
- `src/generic/` 是历史过渡件（material/sampler/vertex），路径追踪改造时一并数据化。
- 根级残留待 Phase 6 归位：`command_manager.*`（CommandPool）、`render_context.hpp`（RenderContext 聚合）。

## 4. 模块地图

### platform/（Layer 1）
| 模块 | 职责 |
|---|---|
| `platform::Window` | GLFW 唯一封装：resize 事件钩子 + 滚轮累积（`consumeScrollDelta`）、`requiredInstanceExtensions()`、`createSurface()`、运行时 `setTitle()`。鼠标/光标事件钩子已移除，输入全部改轮询（目标态，见 input-action-layer-plan.md） |
| `platform::Input` | 轮询式输入门面：`Key/MouseButton` 枚举、`poll()`、电平查询 `isKeyDown/isMouseDown`、指针 delta 与滚轮增量（值流）；**无物理边沿 API**——边沿语义归 `app::ActionContext`。键位集按计划增 `Digit1..9/KP5/Delete`（目标态） |
| `platform::Log` + `LogLocator` | 全项目唯一 Service Locator：`Log` 接口 + `ConsoleLog`/`NullLog`（Null Object） |
| `platform::PlatformUtils` | `assetRoot()/assetPath()`：消灭 `../` 相对路径 |

### rhi/（Vulkan 基础设施）
| 模块 | 职责 |
|---|---|
| `rhi::VulkanDevice` | Instance/PhysicalDevice/Device/队列/MSAA 探测；`CreateInfo` 注入扩展与验证层；`renderContext()` 打包上下文 |
| `rhi::VmaContext/VmaBuffer/VmaImage` | VMA RAII（move-only、映射） |
| `rhi::RhiFactory` | 非单例辅助：ImageView、sync2 屏障（`imageBarrier`）、上传路径 sync1 过渡、格式探测 |
| `rhi::DebugMessenger` / `rhi::Surface` | 原 `Context` 拆分件；验证回调节耦 / 窗口表面 |
| `rhi::Swapchain` | swapchain + MSAA color/depth；深度格式单一探测定点 `depthFormat()`；present mode 由 RenderSettings 决定 |
| `CommandPool`（根级暂留） | 命令池 + 队列 + 单次提交辅助 |

### resource/（Layer 2）
| 模块 | 职责 |
|---|---|
| `MeshData/ImageData` + `MeshImporter/TextureImporter` | CPU 资产与解析（OBJ + glTF 2.0 via tinygltf3；stb 贴图）；`MeshData::postProcess()` 去重/法线/切线 |
| `resource::UploadQueue` | 单次提交上传（staging→device）；mipmap 生成；**自持 VmaAllocator** |
| `resource::ResourceRegistry` | GPU 资产唯一属主（id→MeshGPU/TextureGPU）+ 内建 1×1 默认纹理（Null Object 回落） |
| `resource::AssetLibrary` + `AssetHandle` | 路径缓存 + 引用计数；空句柄 id=0 即 Null Object；`asset_handle.hpp` 为纯 CPU 头，跨层包含不牵连 GPU 声明 |

### render/（Layer 3）
| 模块 | 职责 |
|---|---|
| `render::Renderer` | **仅帧编排**：`beginFrame()→record(ctx, span<RenderItem>)→endFrame(ctx)`；swapchain 重建；`Dependencies` 聚合注入（含内容层下发的 `FrameParams`） |
| `render::FrameResources` | per-frame 状态唯一属主：UBO、Set0、命令缓冲、binary+timeline 同步；`kMaxFramesInFlight=2` |
| `render::CommandRecorder` | 无状态录制：3 次布局过渡 → dynamic rendering → Set0/逐项 Set1 + push flags + 实例绘制 |
| `render::RenderItem` | **场景↔渲染契约**：扁平 POD {MeshGPU*, Material*, InstanceBuffer*, firstInstance, instanceCount}，无 Vulkan 类型 |
| `render::InstanceBuffer` | 单对象 GPU 实例流（vertex binding 1，每实例一个 mat4）；路径追踪时被 TLAS instance 取代 |
| `render::GraphicsPipelineSpec` + `PipelineCache` + `Pipeline` | 管线状态 POD 化 + 按 spec 缓存创建；顶点输入布局在 `pipeline.cpp` 内部定义 |
| `render::ShaderManager` / `DescriptorSetLayout/Pool` | SPIR-V 读取缓存 / SPIRV-Reflect 自动布局与池估算 |
| `render::FrameUniforms` | `UniformBufferObject/Light/FrameParams` 唯一定义处（static_assert 192B 对齐 shader） |
| `render::RenderSettings` | MSAA + present mode（Config 注入、按设备上限钳制） |

### scene/（Layer 4，头文件零 Vulkan/GLFW —— grep 验收项）
| 模块 | 职责 |
|---|---|
| `scene::Scene` | 对象列表属主；`collectRenderItems()` 直产 `std::span<const RenderItem>`（dirty 缓存，帧间零分配） |
| `scene::SceneObject` | 纯数据可绘制体：mesh 句柄（保活）+ 材质 + `Transform` + 本地实例数组；`setInstances()` 合成 world = transform × instance 后经 UploadQueue 上传 |
| `scene::Transform` | TRS + `toMatrix()`；`worldMatrix()` dirty-flag 惰性缓存 |
| `scene::Camera` | 球坐标相机**纯数学**：`orbit/moveHorizontal/moveVertical/zoom/viewMatrix/position`；自带投影参数（`Projection`：透视 fov / 正交视高 + near/far）与 `projectionMatrix(aspect)`；无输入依赖。计划增 `pan(dx,dy,viewportHeight)` 视平面平移（Blender 式，目标态） |
| `scene::CameraManager` | 相机列表属主（deque 条目含名称；add 不失效引用）+ 活动索引：`add(camera, name=""/自动自增 Camera N)/setActive/active/removeActive/name`；纯数据零 Vulkan/GLFW，Renderer 每帧读 `active()`。`cycle()` 已删（目标态） |

### app/（Layer 5）
| 模块 | 职责 |
|---|---|
| `app::App` | **组合根**：`App()` 按层装配（window→rhi→resources→samplers→content→render）+ `initInputBindings()`（注册动作层键位表），`run()` 只做循环转发：update = `actions_.update(input_)` → `cameraController_.update(actions_, input_, dt)`；成员析构逆序（池先于 set 纪律） |
| `app::GameLoop` | 帧节奏：pollEvents → input.poll → deltaTime → update/render 回调；固定步长骨架已预留 |
| `app::ActionContext` | **语义动作层**（边沿语义唯一归属，L1 无边沿）：有序 `Binding{source(Key/MouseButton variant), mods, action, Trigger::Hold/Press, Mode::Default/Walk, param}` + `bind(...)`；每帧 `update(input)`：模态探测（walk>pan>orbit 独占）→ 门控电平求值（Walk 组仅在 walk 模态激活时）→ per-binding 沿计算；查询 `isActive/wasActivated/wasDeactivated/param`（目标态，见 input-action-layer-plan.md） |
| `app::CameraController` | 语义动作→活动相机映射（状态机在 app，相机保持纯数学）：MMB 拖拽轨道、Shift+MMB 视平面平移、Shift+RMB+WASD 漫游、Shift+D 克隆、数字键 1-9 直选、KP5 透视⇄正交、Delete 删除（剩 1 忽略）、滚轮缩放；同步窗口标题 `活动相机名 [Persp\|Ortho]`。只消费 ActionContext 查询与 Input 值流，不读裸 Key（目标态） |
| `app::DemoScene` | 示例内容：三个材质（空句柄回落默认纹理）、火星 + 1000 随机岩石；持有 `FrameParams`（灯光——原 renderer 字面量收编于此；投影参数已 per-camera 移入 `scene::Camera`） |
| `app::Config` | 全部内容参数：窗口、验证层/设备扩展、MSAA、present mode、资产路径（构造时解析为绝对路径） |

`main/main.cpp` 仅构造 `app::App` → `run()`，异常捕获。

## 5. 关键数据流

```
装配（App 构造，自下而上）
window → VulkanDevice → VMA → RhiFactory/ShaderManager → DebugMessenger/Surface → CommandPools
      → UploadQueue → ResourceRegistry → AssetLibrary → Samplers
      → DemoScene.build()（材质 + SceneObject + InstanceBuffer 上传）
      → DescriptorSetLayout → DescriptorPool → 材质 Set-1（每材质一次）
      → Renderer（依赖聚合：RenderContext/池/layout/CameraManager/FrameParams/surface/window）

每帧（GameLoop）
pollEvents → input.poll → update(dt)（ActionContext::update(input) → CameraController 语义消费 → scene::CameraManager.active()；键位表见 docs/input-action-layer-plan.md）
  → beginFrame: waitFence → acquire(OUT_OF_DATE⇒重建并跳帧) → fillUBO(activeCamera+FrameParams) → writeSet0
  → record: scene_.collectRenderItems()（缓存 span）→ CommandRecorder（过渡→Rendering→逐 item）
  → endFrame: timeline+binary submit → present（resize/OOD ⇒ 重建）

资产：loadMesh/loadImage → Importer → MeshData/ImageData → Registry.create*GPU（UploadQueue 上传）→ AssetHandle
```

## 6. 工程约定（代理必须遵守）

1. **C++20 + C++ Core Guidelines**：designated initializers、`std::span/string_view`、`[[nodiscard]]`、聚合代替多参构造、RAII move-only（`VmaBuffer` 等禁拷贝）。
2. **固定宽度整型不加 `std::` 前缀**：写 `uint32_t`/`uint64_t`/`uint8_t`，不写 `std::uint32_t`。
3. **依赖只向下**：`scene/` 头文件禁止 Vulkan/GLFW 类型（验收命令：`grep -rniE "vulkan|glfw|vk::|VkBuffer" src/scene/*.hpp` 应零命中，注释除外）；层间通信走契约类型（`RenderItem`/`AssetHandle`/`FrameContext`/`FrameParams`）。
4. **无单例**：仅 `platform::LogLocator`（带 Null Object）；其余一律构造注入。
5. **App 成员声明顺序 = 析构逆序纪律（曾出过真实事故）**：一切（传递性地）持有 GPU 资源的成员——VMA 分配（`scene_` → InstanceBuffer → VmaBuffer）、`vk::raii` 句柄（材质描述符集、Renderer 的 per-frame set）——必须声明在其依赖的 rhi/resource 成员（`vulkanDevice_`/`vmaContext_`/`descriptorPool_`）**之后**。声明反了 = 分配器/池先死，退出时 VMA 断言 `Some allocations were not freed before destruction of this memory block`。新增持有 GPU 资源的成员时先检查声明位置。
6. **vulkan-hpp RAII 语义**：raii 对象本身传参（如 `vulkanDevice_.instance`），不要 `*` 解引用成 plain handle；raii 创建方法可 const 调用。
7. **每阶段/每次重构的验证协议**：Debug + Release 双配置构建 → **优雅关闭冒烟**（强杀进程不跑析构函数，测不出退出期资源错误，必须走 `CloseMainWindow`）：
   ```powershell
   Set-Location bin; $p = Start-Process -FilePath '.\main.exe' -PassThru
   Start-Sleep -Seconds 6; $null = $p.CloseMainWindow()
   if (-not $p.WaitForExit(15000)) { $p.Kill(); 'DID-NOT-EXIT' } else { "CLEAN-EXIT code=$($p.ExitCode)" }
   ```
   通过标准：`CLEAN-EXIT code=0` 且无 validation/VMA 断言输出（沙箱里 bash 直接拉起 exe 会静默 127，一律用 PowerShell）。→ 用户做黄金帧目视对比。
8. **Git**：全局 `commit.gpgsign=true`（SSH key 带 passphrase），非交互提交用 `git -c commit.gpgsign=false commit ...`；事后补签：`git rebase <base>~1 --exec "git commit --amend --no-edit -S"`。

## 7. 当前状态与下一步

- ✅ Phase 0–5：平台层 / 资源层 / 渲染层拆分 / rhi 归位 / 场景层纯数据化 / 应用层成形。
- ⏳ **Phase 6 · 工程纪律**（唯一剩余重构阶段）：CMake 拆 5 个静态库（编译期强制依赖方向）、显式源文件列表、根级 `command_manager.*`/`render_context.hpp` 归位、`VULKAN_HPP_*` 宏收敛单一公共头、头文件卫生、可选 CI/测试。
- ⏳ **输入语义动作层重构（计划已批准，实施未开始）**：目标态（键位表 / ActionContext / 相机命名与删除 / Window 鼠标钩子裁剪）见 `docs/input-action-layer-plan.md`；§4 相关行即按该目标态描述，**落地前以 `src/` 为准**——当前树处中间态：`CameraController` 仍引用已删的 `Input::wasKeyPressed`，暂不可编译。
- 🎯 **重构完成后**：按计划文档 §6 路线图迁移到 Vulkan 光线追踪（NVIDIA）：`rhi::AccelerationStructure`（BLAS 引用 `MeshGPU` 顶点/索引，TLAS instance 用 `SceneObject::worldMatrix()`）、`RayTracingPipelineSpec`+SBT、trace pass 接入现有 `beginFrame/record/endFrame` 编排、离线累积输出。最终形态 = 纯路径追踪离线渲染器。
- 已知债务清单见 `docs/architecture-current.md` §6（Material 仍 GPU 侧过渡件、`setTransform` 后需重新 `setInstances`、random_device 种子不可复现等）。
