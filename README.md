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
> - [ ] Metallic Texture
> - [ ] Roughness Texture
> - [ ] Vulkan Ray Tracing (Acceleration Structure + RT Pipeline, post-refactor; NVIDIA GPU)
> - [ ] Final target: Path-Tracing

##

(*Based on* **Cpp 20**. & VulkanSDK-1.4.341.1)

**Notice:**

This program uses external libraries [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect).

**If ```SPIRV_REFLECT_USE_SYSTEM_SPIRV_H``` is defined, make sure the include path to the SPIRV-Headers is set correctly.**

---

### Introduction
>
> *The engines don’t move the ship at all. The ship stays where it is and the engines move the universe around it.*
>
> *Futurama*
