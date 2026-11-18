#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "render/material/material.hpp"

namespace resource {
class ResourceRegistry;
}  // namespace resource

namespace render {

// Single owner of every Material used by a frame, and the source of the dense
// MaterialId sequence (creation order, starting at 0). Ids are the draw list's
// deterministic sorting key and the handle per-material state (descriptor set,
// future transparency ordering) is attached to.
//
// Materials live on the heap and are never moved, so a Material* returned by
// create()/get() stays valid until the registry is destroyed. The registry does
// not own GPU descriptor sets: the composition root allocates Set 1 and attaches
// it via Material::attachSet().
//
// Main thread only, like the rest of the render layer.
class MaterialRegistry final {
public:
    MaterialRegistry() = default;
    MaterialRegistry(const MaterialRegistry&) = delete;
    MaterialRegistry& operator=(const MaterialRegistry&) = delete;

    // Creates a material and assigns it the next id. Arguments are forwarded to
    // the Material constructor.
    [[nodiscard]] Material& create(Material::SlotHandles handles,
                                   const resource::MaterialData& data,
                                   const Sampler& texSampler,
                                   const Sampler& norSampler,
                                   const resource::ResourceRegistry& registry);

    // Throws std::out_of_range when no material has this id.
    [[nodiscard]] Material&       get(MaterialId id);
    [[nodiscard]] const Material& get(MaterialId id) const;

    [[nodiscard]] size_t size() const noexcept { return materials_.size(); }
    [[nodiscard]] bool   empty() const noexcept { return materials_.empty(); }

    // Creation order: materials()[i] has id i.
    [[nodiscard]] const std::vector<std::unique_ptr<Material>>& materials() const noexcept {
        return materials_;
    }

private:
    std::vector<std::unique_ptr<Material>> materials_;
};

}  // namespace render
