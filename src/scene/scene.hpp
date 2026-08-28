#pragma once

#include <memory>
#include <span>
#include <vector>

#include "render/render_item.hpp"  // the layer contract; Vulkan-free by design
#include "scene/scene_object.hpp"

namespace scene {

// Owns the drawables and produces the render layer's draw list. Zero
// Vulkan/GLFW types: the product is render::RenderItem, a flat POD holding
// mesh/material/instance-stream handles only.
class Scene {
public:
    void addObject(std::shared_ptr<SceneObject> object);

    bool isValid();
    void clear();

    // One RenderItem per drawable object (instances > 0). Returns a view into
    // an internal cache that is rebuilt only after addObject — no per-frame
    // allocation; valid until the next scene mutation.
    [[nodiscard]] std::span<const render::RenderItem> collectRenderItems() const;

    [[nodiscard]] const std::vector<std::shared_ptr<SceneObject>>& objects() const { return objects_; }

private:
    std::vector<std::shared_ptr<SceneObject>> objects_;

    mutable std::vector<render::RenderItem> cachedItems_;
    mutable bool itemsDirty_ = true;
};

}  // namespace scene
