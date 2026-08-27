#pragma once

#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace resource {
class MeshGPU;
}  // namespace resource

class Material;  // global until the Phase 4 domain sweep renames it render::

namespace render {

// Pure-data draw unit: the scene side fills these, CommandRecorder consumes
// them. Kept as a flat POD on purpose — zero virtuals and no pointer chasing
// inside the hot recording loop (plan §2.6, data-oriented design).
//
// Transitional shape (Phase 3): raw mesh/material pointers plus the object's
// instance buffer. Phase 4 replaces them with registry handles once
// Scene::collectRenderItems becomes the producer.
struct RenderItem {
    const resource::MeshGPU* mesh         = nullptr;
    const Material*          material     = nullptr;
    vk::Buffer               instanceBuffer{};  // raw VkBuffer handle
    uint32_t            instanceCount = 0;
    uint32_t            firstInstance = 0;
};

}  // namespace render
