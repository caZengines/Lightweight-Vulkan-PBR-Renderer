#include "scene/scene.hpp"
#include "platform/log.hpp"
#include "scene/primitive_batching.hpp"
#include "render/material/material.hpp"

namespace scene {

void Scene::addObject(std::shared_ptr<SceneObject> object) {
    objects_.emplace_back(std::move(object));
    itemsDirty_ = true;
}

bool Scene::isValid() {
    return !objects_.empty() || !cachedItems_.empty();
}

void Scene::clear() {
    objects_.clear();
    cachedItems_.clear();
    itemsDirty_ = true;
    platform::LogLocator::get().write(platform::LogLevel::Info, "[scene] scene cleaned");
}

std::span<const render::RenderItem> Scene::collectRenderItems() const {
    if (itemsDirty_) {
        cachedItems_.clear();
        size_t itemCount = 0;
        for (const auto& object : objects_) {
            itemCount += object->instanceCount();
        }
        cachedItems_.reserve(itemCount);

        for (const auto& object : objects_) {
            const uint32_t instanceCount = object->instanceCount();
            if (instanceCount == 0) continue;

            if (object->material().isBlend()) {
                // Blend draws are one instance each: the renderer reorders them
                // every frame by their world centers, which is only possible if
                // each instance is its own RenderItem.
                for (uint32_t i = 0; i < instanceCount; ++i) {
                    cachedItems_.emplace_back(render::RenderItem{
                        .mesh          = &object->mesh(),
                        .material      = &object->material(),
                        .instances     = &object->instanceBuffer(),
                        .firstInstance = i,
                        .instanceCount = 1,
                        .worldCenter   = object->instanceWorldCenter(i),
                    });
                }
            } else {
                cachedItems_.emplace_back(render::RenderItem{
                    .mesh          = &object->mesh(),
                    .material      = &object->material(),
                    .instances     = &object->instanceBuffer(),
                    .firstInstance = 0,
                    .instanceCount = instanceCount,
                    .worldCenter   = object->averageWorldCenter(),
                });
            }
        }
        batchRenderItems(cachedItems_);
        itemsDirty_ = false;
    }
    return cachedItems_;
}

}  // namespace scene
