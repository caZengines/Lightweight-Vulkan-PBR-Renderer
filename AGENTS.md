# AGENTS.md — Vulkan 渲染器 · 架构与工作指南

> 本文件是仓库的活地图与硬性规则，供 AI 代理在动手前快速定位、避免踩坑。代码与本文冲突时以 `src/` 为准。
>
> 历史设计记录见 `docs/`：`pipeline-descriptor-refactor-plan.md`（描述符/管线重构，主体已落地）、
> `light-system-plan.md`（多光源，未实施）、`future-architecture.md`（RT 模式 / shader 模块化 / 热重载，未实施）。

## 1. 项目定位

- 学习型 Vulkan PBR 渲染器，路线终点是**纯路径追踪离线渲染器**（目标硬件 NVIDIA GPU）。
- 当前形态：光栅 PBR（glTF 资产、MSAA、present timing 限帧）+ 一条**临时的**计算管线（粒子模拟，仅用于打通 compute 路径，后续会被替换）。
- 技术栈：C++20（C++ Core Guidelines）、vulkan-hpp RAII、VMA、Slang → SPIR-V、GLFW、glm；第三方源码在 `src/extern/`，Slang 可复用模块在 `shaders/modules/`。
- 构建形态：单可执行目标 `main`，源列表由 `GLOB_RECURSE` 生成；Phase 6 的静态库拆分**尚未开始**。

## 2. 构建、运行、验证

```bash
# 配置 + 构建（Ninja；两个构建树并存，产物都是 bin/main.exe）
cmake -S . -B cmake-build-Debug   -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-Debug
cmake -S . -B cmake-build-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-Release

# 运行：cwd 必须是 bin/（PlatformUtils 以 cwd 的父目录为 asset root）
cd bin && ./main.exe
```

- **改过 `src/` 下的文件名/目录后必须重新 configure**：`GLOB_RECURSE` 在 configure 时求值，不 reconfigure 会静默漏编新文件。
- shader 编译由 CMake 自定义命令完成（`slangc ... -I shaders/modules`），随构建执行，不要手工产出 `.spv`。
- **bin/ 自包含**：POST_BUILD 会把 ucrt64 的 `libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll / glfw3.dll` 拷到 exe 旁。PATH 里的 mingw64 版运行库会导致 `STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139)`。
- `bin/main.exe` 被占用时链接失败——先结束残留进程。
- 从备份恢复文件会保留旧 mtime，ninja 可能跳过编译；恢复后 touch 一下目标文件。

**每次改动的验证协议**（缺一不可）：

```powershell
Set-Location bin
$p = Start-Process -FilePath '.\main.exe' -PassThru -RedirectStandardOutput out.txt -RedirectStandardError err.txt
Start-Sleep -Seconds 10
$null = $p.CloseMainWindow()
if (-not $p.WaitForExit(15000)) { $p.Kill(); 'DID-NOT-EXIT' } else { "CLEAN-EXIT code=$($p.ExitCode)" }
```

通过标准：`CLEAN-EXIT code=0`，stderr 无 validation / VMA 断言输出。**必须走 `CloseMainWindow`**（强杀不跑析构，测不出退出期资源错误）；沙箱里用 bash 直接拉起 exe 会静默 127，一律用 PowerShell。

## 3. 目录与依赖规则

五层，依赖只允许向下；层间通信只能通过契约类型（`RenderItem` / `AssetHandle` / `FrameParams` / `RenderTarget` 等）。

```
Layer 5  app/        组合根、主循环、输入语义映射、示例内容      (app::)
Layer 4  scene/      纯数据场景 + 相机数学，零 Vulkan/GLFW       (scene::)
Layer 3  render/     帧编排、pass 录制、管线、描述符、每帧状态    (render::)
Layer 2  resource/   资产导入、GPU 资产注册表、上传队列、shader   (resource::)
Layer 1  platform/   窗口/输入/日志/路径/CPU 等待（GLFW 唯一出入处）(platform::)
Layer 0  rhi/        Vulkan 封装：设备/交换链/VMA/描述符/命令池    (rhi::)
```

| 层 | 子目录 |
|---|---|
| `src/app/` | 扁平：`app` `config` `game_loop` `action_context` `camera_controller` `demo_scene` |
| `src/scene/` | 扁平：`scene` `scene_object` `transform` `camera` `camera_manager` |
| `src/render/` | 根：`renderer.*`（层入口）`material.*`（材质数据+Set1 写入）· `gpu/`（渲染层自建常驻 GPU 数据，现为 `InstanceBuffer`）· `frame/`（每帧轮转状态与跨层契约：`frame_resources` `frame_uniforms` `render_item` `render_settings` `render_target`）· `pass/`（`pass` 抽象、`raster_pass`、`command_recorder`）· `pipeline/`（光栅管线族 + 两族共用：`graphics_pipeline`+spec、`pipeline_base`、`graphics_pipeline_library`、`pipeline_layout_library`、`shader_stage`）· `compute/`（计算管线族：`compute_pipeline`+spec、`compute_pipeline_library`、`compute_pass`、`compute_resources`）· `descriptor/`（`descriptor_budget`、`descriptor_set_layout_library`） |
| `src/resource/` | `import/`（`gltf_importer` `mesh_importer` `texture_importer`）· `data/`（`mesh_data` `image_data` `vertex`）· `asset/`（`asset_handle` `asset_library` `resource_registry`）· `gpu/`（`upload_queue` `shader_library` `sampler`） |
| `src/rhi/` | `device/`（`vulkan_device` `debug_messenger` `rhi_factory` `render_context`）· `present/`（`surface` `swapchain`）· `descriptor/`（`descriptor_binding` `descriptor_set_id` `descriptor_write` `descriptor_set_allocator` `descriptor_writer`）· `memory/`（`vma_allocator`）· `command/`（`command_pool`） |
| `src/platform/` | `window` `input` `log` `utils` `cpu` · `src/main/main.cpp` 只构造 `app::App` · `src/extern/` 为 vendored C 源码，不要改 |

**命名与 include**：

- 命名空间 = 层，**不加子命名空间**（目录只做物理分组）。
- include 一律从 `src/` 根写全路径：`#include "rhi/descriptor/descriptor_writer.hpp"`；不把子目录塞进 include dirs。
- 单模块目录允许存在（表达「将来会长大的子系统」），不要为凑数拆目录。

**可 grep 验收的不变量**：

| 不变量 | 验收 |
|---|---|
| scene 头文件零 Vulkan/GLFW | `Select-String -Path src/scene/*.hpp -Pattern 'vulkan\|glfw\|vk::\|VkBuffer'` 应只命中注释 |
| rhi 不依赖上层 | `rhi/` 内不得 `#include "resource/…"` / `"render/…"`。**现存例外**：`present/swapchain` 为取 present mode / MSAA 直接依赖 `render::RenderSettings`（待解耦，见 §8） |
| resource 不碰描述符类型 | `resource/` 内不得出现 `DescriptorBinding/DescriptorWriter/DescriptorSetAllocator` |
| render 不读盘 | `render/` 内不得出现 `ifstream/ofstream/fstream`（SPIR-V 读取在 `resource/gpu/shader_library`） |
| descriptor set 所有权单一 | `vk::raii::DescriptorSet` 只允许出现在 `rhi/descriptor/descriptor_set_allocator.*`；外部持 `rhi::DescriptorSetId` + 裸句柄 |

## 4. 模块地图（按层）

### platform/
| 模块 | 职责 / 契约 |
|---|---|
| `platform::Window` | GLFW 唯一封装：resize 钩子、滚轮累积、`requiredInstanceExtensions()`、`createSurface()`、`setTitle()`。输入事件全部走轮询 |
| `platform::Input` | 轮询门面：键/鼠标电平、指针 delta、滚轮增量；**无物理边沿 API** |
| `platform::Log` + `LogLocator` | 全项目唯一 Service Locator：`ConsoleLog`/`FileLog`（线程缓冲批写）/`NullLog`；`App` 启动时 `provide(&fileLog)` |
| `platform::PlatformUtils` | `assetRoot()`（cwd 的父目录）/`assetPath()`；消灭 `../` 相对路径 |
| `platform::sleepUntil` / `relax` | 高精度定时（Windows 用 high-resolution waitable timer，余量自旋）；给 CPU pacer 用 |

### rhi/
| 模块 | 职责 / 契约 |
|---|---|
| `rhi::VulkanDevice` | Instance/PhysicalDevice/Device/队列/MSAA 探测；`CreateInfo` 注入扩展与验证层；`renderContext()` 打包 `RenderContext` |
| `rhi::VmaContext/VmaBuffer/VmaImage` | VMA RAII（move-only、map/unmap、`mappedData()` 仅限 MAPPED 分配） |
| `rhi::RhiFactory` | ImageView、sync2 图像/缓冲区屏障（`imageBarrier`/`bufferBarrier`）、上传路径过渡、格式探测 |
| `rhi::DebugMessenger` / `rhi::Surface` | 验证回调 / 窗口表面封装 |
| `rhi::Swapchain` | swapchain + MSAA color/depth 附件的属主；`depthFormat()` 是深度格式唯一探测定点；present timing 能力查询 |
| `rhi::CommandPool` | 命令池 + 队列 + 单次提交辅助（一次命令的 begin/end 封装） |
| `rhi::DescriptorSetAllocator` | 描述符池分板 + set 属主；发 `DescriptorSetId`，`release(id)` 归还；**必须等引用该 set 的已提交命令执行完**才能释放 |
| `rhi::DescriptorWriter` | 表驱动写入：按 layout 表逐 binding 用**名字**解析值；漏写、类型/数量不符直接抛 |
| `rhi::DescriptorBinding` | `vk::DescriptorSetLayoutBinding` + 反射名字（单表，名字与 binding 同结构体） |

### resource/
| 模块 | 职责 / 契约 |
|---|---|
| `MeshData/ImageData` + `MeshImporter/TextureImporter` | CPU 资产与解析（OBJ、glTF 2.0 via tinygltf、stb 贴图）；`MeshData::postProcess()` 去重/法线/切线 |
| `resource::UploadQueue` | 单次提交上传（staging→device）、mipmap 生成；自持 allocator；**目标 buffer 必须带 `eTransferDst`** |
| `resource::ResourceRegistry` | GPU 资产唯一属主（id→MeshGPU/TextureGPU）+ 内建 1×1 默认纹理（Null Object） |
| `resource::AssetLibrary` + `AssetHandle` | 路径缓存 + 引用计数；空句柄（id=0）即 Null Object；`asset_handle.hpp` 为纯 CPU 头 |
| `resource::ShaderLibrary` | SPIR-V 唯一读取入口 + SPIRV-Reflect 反射（binding、push constant、入口点、stage） |
| `resource::Sampler` | 采样器包装（albedo 重复寻址 / normal 各向异性） |

### render/
| 模块 | 职责 / 契约 |
|---|---|
| `render::Renderer` | **仅帧编排**：`beginFrame() → record(ctx, items) → endFrame(ctx)`；持 `Dependencies` 聚合；swapchain 重建；内嵌 `PresentTimingController` |
| `render::FrameResources` | 每帧轮转状态属主：UBO×2、Set0、命令缓冲、binary+timeline 同步；`kMaxFramesInFlight = 2` |
| `render::RenderItem` | 场景↔渲染契约：扁平 POD（MeshGPU\*、Material\*、InstanceBuffer\*、firstInstance、instanceCount），无 Vulkan 类型 |
| `render::Material` | 材质数据 + Set1 的两个**数组值**（maps/samplers，按 `MaterialTextureSlot` 索引）+ push constant 块；不拥有 set（持 id + 裸句柄） |
| `render::GraphicsPipeline` / `ComputePipeline` | 两族管线，共用 `PipelineBase`（句柄/绑定点/push-constant stages）；spec 是缓存键 |
| `render::GraphicsPipelineLibrary` / `ComputePipelineLibrary` | 按 `(spirvPath, layout, spec)` 缓存，返回引用活到库析构 |
| `render::GraphicsPipelineSpec` | 值类型：附件格式、固定功能状态、**`vertexLayout`**（数据化的顶点输入）、两个入口点名 |
| `render::PipelineLayoutLibrary` | 按 `(spirvPaths, pushConstantRanges)` 缓存 `vk::PipelineLayout`，set layout 与描述符表同源 |
| `render::DescriptorSetLayoutLibrary` | 手工 Set0 + 反射 Set1+ 的 `LayoutSet{bySetIndex, bindingTables}`，无洞 set 表 |
| `render::PoolBudgetBuilder` | `bindingTables × 同时在用的 set 数` → `rhi::DescriptorPoolBudget` |
| `render::Pass` / `RasterPass` / `ComputePass` | `PassContext{cmd, target, frameSet}`；光栅 pass 做 dynamic rendering（mesh + 临时粒子点），计算 pass 绑 Set2 + push constants + dispatch |
| `render::CommandRecorder` | 帧级命令生命周期与图像布局转换（begin: Undefined→attachment；end: →PresentSrc） |
| `render::FrameUniforms` | `Light`/`UniformBufferObject`（192B，static_assert 对齐 shader）/`FrameParams` 唯一定义处 |
| `render::InstanceBuffer` | 单对象实例流（vertex binding 1，每实例一个 mat4）；`render/gpu/` |
| `render::ComputeResources` | **临时的粒子模拟状态**：2 个 ping-pong buffer + 2 个描述符集（读/写方向），parity 每帧翻转；含 compute→compute 屏障 |

### scene/
| 模块 | 职责 / 契约 |
|---|---|
| `scene::Scene` | 对象列表属主；`collectRenderItems()` 直产 `std::span<const RenderItem>`（dirty 缓存，帧间零分配） |
| `scene::SceneObject` | 纯数据可绘制体：mesh 句柄（保活）+ `Material*` + `Transform` + 本地实例数组；`setInstances()` 合成 world 矩阵并经 UploadQueue 上传 |
| `scene::Transform` | TRS + `toMatrix()`；`worldMatrix()` dirty 懒缓存 |
| `scene::Camera` | 球坐标相机纯数学（orbit/move/zoom/view/projection），自带投影参数 |
| `scene::CameraManager` | 相机列表属主（deque，add 不失效引用）+ 活动索引 + 命名 |

### app/
| 模块 | 职责 / 契约 |
|---|---|
| `app::App` | 组合根：`initWindow → initRhi → initActionContext → initResources → initSamplers → initContent → initRender`；`run()` 只转发循环 |
| `app::GameLoop` | pollEvents → input.poll → deltaTime → update/render 回调；skip 帧由回调返回值控制 |
| `app::ActionContext` | 语义动作层（边沿语义唯一归属）：`Binding{source, mods, action, Trigger, Mode}`；模态门控（walk 组仅 Walk 模式激活） |
| `app::CameraController` | 语义动作→相机状态机：MMB 轨道、Shift+MMB 平移、Shift+RMB 漫游（WASD/QE）、KP5 透视⇄正交；所有输入状态留在 app 层，`scene::Camera` 只做数学 |
| `app::DemoScene` | 示例内容：加载 glTF（每个 primitive 一个 `SceneObject`，每个 glTF 材质一个 `Material`，`materials_[0]` 为无材质 primitive 的兜底）+ `FrameParams`（灯光由内容层下发） |
| `app::Config` | 全部内容参数：窗口、验证层、设备扩展、MSAA、present mode、targetFPS、资产路径（构造时解析为绝对路径） |

## 5. 关键子系统与不变量

### 5.1 描述符
1. `DescriptorSetLayoutLibrary::layoutSetFor(paths)` 产出 `LayoutSet`：`bySetIndex[i]` 是 set i 的 layout，`bindingTables[i]` 是同序的 `rhi::DescriptorBinding` 表（名字来自 SPIR-V 反射；手工 Set0 的名字为空，只能按编号写）。
2. 池预算 = `PoolBudgetBuilder`（每条表 × 同时在用数量）→ `DescriptorSetAllocator`。单个分板用尽时分配器会按同一预算追加分板；只有**单个 set 无法被满足**（类型不在预算里，或数量超过 `maxSets`）才抛异常。新增 set/binding 时必须同步预算与用途数量（`app.cpp::initRender`）。
3. 写入一律 `DescriptorWriter::writeAll(values) + flush()`：值用 `bufferWrite(name, infos)` / `imageWrite(name, infos)` 按**反射名字**匹配，元素数必须等于 `descriptorCount`。
4. `Material` 只持 `DescriptorSetId` + 裸句柄；释放必须推迟到引用它的帧执行完（`kMaxFramesInFlight`），不要在删除点直接 `release`。

### 5.2 管线
- `PipelineBase` 提供 `binding/layout/bindPoint/pushConstantStages`；`GraphicsPipeline` 与 `ComputePipeline` 只负责各自的创建逻辑。
- 顶点输入是 **spec 的数据**（`VertexLayoutSpec{bindings, attributes}`），不再写死在 `graphics_pipeline.cpp`；空 layout = 不读顶点缓冲（点/全屏绘制）。
- 管线布局由 `PipelineLayoutLibrary` 按（SPIR-V 路径集合, push constant 范围）缓存；图形管线布局**包含 compute shader 声明的 set**，因此光栅 pass 可以绑 Set2。
- 同一 SPIR-V 可含多个入口点；spec 的 `vertEntry/fragEntry` 选择入口，缓存键因此不同。

### 5.3 帧与同步
- `FrameResources` 每个 slot 一套 UBO/Set0/命令缓冲/fence；`beginFrame` 先等该 slot 的 fence，再 acquire。
- 提交：timeline semaphore（每帧递增）+ binary semaphore（present 等待）。
- 跨帧 buffer 复用依赖 slot fence；**跨阶段（compute→graphics）依赖必须显式加 `bufferBarrier`**（`Renderer::record` 里有现成写法）。
- 帧节奏：`paceBeforeSubmit()` 负责平均节奏（绝对 deadline，晚到则重锚定），`PresentTimingController::targetDuration()` 只负责对齐 vblank。FRR 下目标帧率应整除刷新率，否则会周期性多出一个刷新周期（见 README TODO）。
- 已知设计问题：pacing 睡眠发生在 `acquire` 之后（会握着 swapchain 图像空等）。

### 5.4 粒子（临时）
- `ComputeResources`：2 个 storage buffer ping-pong + 2 个描述符集（`set[p]` 读 `buffers_[p]`、写 `buffers_[p^1]`），`parity_` 每帧翻转，`advance()` 一帧恰好一次。
- 同步：`recordBarrier()`（compute→compute，覆盖跨帧 RAW/WAR）；dispatch 后 `bufferBarrier`（compute 写 → vertex/fragment 读）。
- 光栅侧由 `RasterPass` 的 `ParticleDraw{pipeline, set, count}` 画出（点拓扑，顶点着色器用 `SV_VertexID` 索引 SSBO）。
- 这套代码属于测试设施，替换粒子方案时连同 `compute/compute_resources.*`、`compute/compute_pass.*`、shader 里的粒子入口、`Renderer` 侧的粒子管线一并删除。

## 6. 关键数据流

```
装配（App 构造，自下而上）
window → VulkanDevice → VMA → RhiFactory → DebugMessenger/Surface → CommandPools(graphics/compute/transient)
      → UploadQueue → ResourceRegistry → AssetLibrary → Samplers
      → ShaderLibrary → DescriptorSetLayoutLibrary → PoolBudgetBuilder → DescriptorSetAllocator
      → DemoScene.build()（材质 + SceneObject + InstanceBuffer 上传）
      → 每材质：allocate Set1 → descriptorWrites() → writeAll → attachSet
      → Renderer(Dependencies)：PipelineLayoutLibrary + FrameResources(Set0/UBO/同步) + 管线库

每帧（GameLoop）
pollEvents → input.poll → actions_.update(input) → CameraController::update → renderer:
  beginFrame: waitFence(slot) → acquire(OOD⇒重建并跳帧) → fillUniformBuffer → writeFrameSet → reset fence/cmd
  record: CommandRecorder::begin(布局转换)
          → [compute] recordBarrier → bind Set2 + push(dt,gravity) → dispatch → advance → bufferBarrier
          → RasterPass: beginRendering → mesh items → 粒子点 → endRendering
          → CommandRecorder::end(→PresentSrc)
  endFrame: paceBeforeSubmit → submit(timeline+binary) → present(present timing) → OOR/suboptimal⇒重建

资产：loadMesh/loadImage → Importer → MeshData/ImageData → ResourceRegistry.create*GPU（UploadQueue）→ AssetHandle
```

## 7. 编码规范（强制）

1. **C++20 + Core Guidelines**：designated initializers、`std::span/std::string_view`、`[[nodiscard]]`、聚合代替多参构造、RAII move-only（`VmaBuffer`/`VmaImage` 禁拷贝）。
2. 固定宽度整型**不加 `std::` 前缀**：`uint32_t`/`uint64_t`/`uint8_t`。
3. **无单例**：仅 `platform::LogLocator`（带 Null Object）；其余一律构造注入。
4. **vulkan-hpp RAII 语义**：raii 对象本身传参（如 `vulkanDevice_.instance`），不要解引用成裸 handle；raii 的创建方法可 const 调用。
5. **App 成员声明顺序 = 析构逆序纪律**：一切（传递地）持有 GPU 资源的成员必须声明在其依赖的 rhi/resource 成员之后（`vulkanDevice_`/`vmaContext_`/`descriptorSetAllocator_`）。反了会在退出时触发 VMA 断言 `Some allocations were not freed before destruction`。新增持有 GPU 资源的成员时先检查声明位置。
6. **依赖只向下**：上层不得被下层 include；跨层只用契约类型。
7. **Git**：全局 `commit.gpgsign=true`（SSH key 带 passphrase）；非交互提交用 `git -c commit.gpgsign=false commit ...`，事后补签 `git rebase <base>~1 --exec "git commit --amend --no-edit -S"`。

### 7.1 注释规范（强制）

- **语言：英文。**
- 只描述**当前**模块/函数的职责与依赖：它做什么、依赖谁、被谁拥有/调用。
- 必要时用 1–2 句说明**当前设计为何如此**：生命周期、线程模型、Vulkan 前置条件、所有权、性能取舍——目的是让读者判断改动的边界。
- **不写改动历史**：不出现「以前是 X」「改成 Y 是为了修 Z」「曾经的实现」这类叙述，也不复述代码本身能表达的逻辑。
- 头文件注释写契约（所有权、生命周期、线程安全、未定义行为）；`.cpp` 注释写局部不变量与 Vulkan 前置条件（如 `VUID`、布局、访问掩码）。
- 参考写法：`rhi/descriptor/descriptor_set_allocator.hpp` 的顶部说明、`render/pass/pass.hpp` 的类注释。

## 8. 当前状态与待办

- ✅ 分层、描述符子系统（手工 Set0 + 反射 Set1+ + 池预算 + 表驱动写入）、光栅/计算两族管线、present timing 限帧、输入语义动作层。
- ⏳ **多光源**（`docs/light-system-plan.md`）：当前只有单个 `Light`（UBO 内联），灯数组需要新增 Set0 binding + 预算 + 反射校验流程。
- ⏳ **计算队列拆分**（README TODO #1）：目前 compute 与 graphics 共用一个队列，compute 只是录进同一帧命令缓冲。
- ⏳ **FRR 限帧改进**（README TODO #2）：目标帧率不整除刷新率时会有周期性顿挫；计划用 present-timing 的累计误差 + `vkGetPastPresentationTimingEXT` 回馈修正。
- ⏳ **Phase 6 工程纪律**：CMake 拆 5 个静态库（把依赖方向变成编译期约束）、显式源文件列表、`VULKAN_HPP_*` 宏收敛到单一公共头。
- ⏳ **粒子系统替换**：当前实现是测试设施（见 §5.4），不保证 API 稳定。
- 🎯 长期路线：`docs/future-architecture.md`（RT opt-in 模式、shader 模块化、热重载），最终形态为纯路径追踪离线渲染器。
- 已知债务：`rhi/present/swapchain` 直接依赖 `render::RenderSettings`（层次回边，应改为 rhi 自己的小型 settings 结构）；`Material` 仍直接持 GPU set（未来应只描述数据）；`SceneObject::setTransform()` 之后需重新 `setInstances()` 才会更新实例矩阵；粒子 dt 在 `ComputePass` 内用静态时钟计算（临时做法，应改由渲染循环传入）；随机种子目前可复现（`mt19937(1337)`），引入物理/路径追踪时保持固定种子。

## 9. 排错速查

| 现象 | 先查 |
|---|---|
| 新文件没被编译 | 重新 `cmake -S . -B cmake-build-Debug`（GLOB 未刷新） |
| 链接失败、exe 被占用 | 结束残留 `main.exe` |
| 启动即 `0xC0000139` | bin/ 下 DLL 被外来 PATH 抢先解析；确认 POST_BUILD 拷贝了 4 个 DLL |
| 退出时 VMA 断言 | `App` 成员声明顺序（§7 第 5 条）或某处 GPU 资源比属主活得久 |
| validation 报描述符错误 | set 表与 shader 反射是否一致（名字/类型/count），预算是否覆盖同时在用的 set 数 |
| 画面撕裂/顿挫 | `targetFPS` 是否整除刷新率；present mode；pacing 与 acquire 的先后 |
| 退出码非 0 且有 stderr | 捕获 `Start-Process -RedirectStandardError`（验证层输出走 stderr） |
