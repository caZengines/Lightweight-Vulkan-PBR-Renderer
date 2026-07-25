#pragma once

#include "generic/renderobject.hpp"

struct DrawBatch {
    std::shared_ptr<const Mesh>     mesh;
    std::shared_ptr<Material>       material;
    vk::Buffer                      instanceBuffer;  //Buffer pointing to RenderObject
    uint32_t                        instanceCount;
    uint32_t                        firstInstance;   // used for indirect draw
};

class Scene {
public:
    //em, just as its name implies
    void addObject(std::shared_ptr<RenderObject> object) { objects_.emplace_back(object); }
    //just as its name implies also
    //void removeObject(size_t index);

    // Build sorted drawing batches
    // Merge instances of the same (Material, Mesh) into one DrawBatch
    std::vector<DrawBatch> buildDrawBatches() const;

    const auto& getObjects() const { return objects_; }

private:
    std::vector<std::shared_ptr<RenderObject>> objects_;
};