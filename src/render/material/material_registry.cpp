#include "render/material/material_registry.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace render {

Material& MaterialRegistry::create(Material::SlotHandles handles,
                                   const resource::MaterialData& data,
                                   const Sampler& texSampler,
                                   const Sampler& norSampler,
                                   const resource::ResourceRegistry& registry) {
    const MaterialId id = static_cast<MaterialId>(materials_.size());
    // The Material constructor is private to the registry, so make_unique cannot
    // be used here.
    materials_.emplace_back(std::unique_ptr<Material>{
        new Material(id, std::move(handles), data, texSampler, norSampler, registry)});
    return *materials_.back();
}

Material& MaterialRegistry::get(MaterialId id) {
    if (id >= materials_.size()) {
        throw std::out_of_range("MaterialRegistry: unknown material id " +
                                std::to_string(id));
    }
    return *materials_[id];
}

const Material& MaterialRegistry::get(MaterialId id) const {
    if (id >= materials_.size()) {
        throw std::out_of_range("MaterialRegistry: unknown material id " +
                                std::to_string(id));
    }
    return *materials_[id];
}

}  // namespace render
