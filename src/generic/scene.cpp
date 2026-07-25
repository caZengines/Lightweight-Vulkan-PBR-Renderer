#include "generic/scene.hpp"

std::vector<DrawBatch> Scene::buildDrawBatches() const {
    std::vector<DrawBatch> batches;
    batches.reserve(objects_.size());

    for (const auto& obj : objects_) {
        if (obj->getInstanceCount() == 0) continue;

        DrawBatch batch{};
        batch.mesh           =  obj->getMeshShared();
        batch.material       =  obj->getMaterialShared();
        batch.instanceBuffer = *obj->getInstanceBuffer();
        batch.instanceCount  =  obj->getInstanceCount();
        batch.firstInstance  =  0;
        batches.push_back(std::move(batch));
    }

    return batches;
}