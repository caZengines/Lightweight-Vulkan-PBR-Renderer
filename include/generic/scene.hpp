#pragma once

#include "generic/renderobject.hpp"

struct DrawBatch {
    std::shared_ptr<const Mesh>     mesh;
    std::shared_ptr<Material>       material;
    VkBuffer                        instanceBuffer;  // Buffer pointing to RenderObject
    uint32_t                        instanceCount;
    uint32_t                        firstInstance;   // used for indirect draw
};

class Scene {
public:
    // em, just as its name implies
    void addObject(std::shared_ptr<RenderObject> object) { objects_.emplace_back(object); batchesDirty_ = true; }
    // just as its name implies also
    // void removeObject(size_t index);

    const std::vector<DrawBatch> getDrawBatches();

    const auto& getObjects() const { return objects_; }

private:
    std::vector<std::shared_ptr<RenderObject>>  objects_{};
    std::vector<DrawBatch>                      cachedBatches_{}; 
    bool                                        batchesDirty_ = true;

    // Build sorted drawing batches
    // Merge instances of the same (Material, Mesh) into one DrawBatch
    std::vector<DrawBatch> buildDrawBatches() const;
};