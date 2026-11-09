# **Vulkan**

 This project is a lightweight Vulkan-based PBR renderer. At present, this project is still in the early stage of framework construction. Graphic interactive interfaces will be implemented in the future.

The following are partially implemented or unimplemented functions:
>
> - [x] Push Constant
> - [x] Instanced Rendering
> - [x] Separate Image and Sampler
> - [ ] Dynamic Uniform Buffer
> - [~~] Frustum Culling (dropped: the project targets pure path tracing)
> - [ ] Multithreaded command buffer generation
> - [x] Shader-reflection automatically generates descriptorset
> - [x] Normal Texture
> - [x] Metallic Texture
> - [x] Roughness Texture
> - [ ] Vulkan Ray Tracing (Acceleration Structure + RT Pipeline, post-refactor; NVIDIA GPU)
> - [ ] Final target: Path-Tracing

##

(*Based on* **Cpp 20**. & VulkanSDK-1.4.351)

**Notice:**

This program uses external libraries [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect).

**If ```SPIRV_REFLECT_USE_SYSTEM_SPIRV_H``` is defined, make sure the include path to the SPIRV-Headers is set correctly.**

**TODO:**
1.Currently, eGraphcis and eCompute share the same queue. They should be separated as much as possible and handle asynchronous submissions.

2.Frame limiter under FRR (fixed refresh rate): a target FPS that does not divide the refresh
rate cannot be presented evenly. 144 fps on a 300 Hz panel is 2.08 refresh cycles, so the
presentation engine inserts a 3-cycle frame roughly every 12 frames and the animation
visibly hitches (150 fps = exactly 2 cycles is smooth). Improve this with the present-timing
path (`VK_EXT_present_timing` + `VK_KHR_present_wait`): derive each frame's target time from
the accumulated presentation error instead of the raw `1/targetFPS` period, and/or read back
`vkGetPastPresentationTimingEXT` to correct the CPU pacer (`Renderer::PresentTimingController`
/ `Renderer::paceBeforeSubmit`).

---

### Introduction
>
> *The engines don’t move the ship at all. The ship stays where it is and the engines move the universe around it.*
>
> *Futurama*
