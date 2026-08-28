#include "scene/scene.hpp"

namespace scene {

void Scene::addObject(std::shared_ptr<SceneObject> object) {
    objects_.emplace_back(std::move(object));
    itemsDirty_ = true;
}

bool Scene::isValid() {
    return !objects_.empty() || !cachedItems_.empty();
}

void Scene::clear() {
    if(!objects_.empty()) objects_.clear();;
    if(!cachedItems_.empty()) cachedItems_.clear();
}

std::span<const render::RenderItem> Scene::collectRenderItems() const {
    if (itemsDirty_) {
        cachedItems_.clear();
        cachedItems_.reserve(objects_.size());
        for (const auto& object : objects_) {
            if (object->instanceCount() == 0) continue;

            cachedItems_.emplace_back(render::RenderItem{
                .mesh          = &object->mesh(),
                .material      = &object->material(),
                .instances     = &object->instanceBuffer(),
                .firstInstance = 0,
                .instanceCount = object->instanceCount(),
            });
        }
        itemsDirty_ = false;
    }
    return cachedItems_;
}

}  // namespace scene
