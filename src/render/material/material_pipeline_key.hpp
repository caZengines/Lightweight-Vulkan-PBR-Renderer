#pragma once

#include <cstdint>
#include "resource/import/gltf_importer.hpp"

namespace render {

enum class MaterialUvSlot : uint32_t {
    BaseColor = 0,
    Normal    = 1,
    Clearcoat = 2,
    Count
};

inline const std::array<std::pair<MaterialUvSlot, resource::MaterialTextureSlot>, 2> kActiveMaterialUvSlots{
    std::pair{MaterialUvSlot::BaseColor, resource::MaterialTextureSlot::BaseColor},
    std::pair{MaterialUvSlot::Normal, resource::MaterialTextureSlot::Normal},
};

inline constexpr size_t kMaterialUVSlotCount = static_cast<size_t>(MaterialUvSlot::Count);
inline constexpr size_t kMaterialPipelineVariantCount = 4;
using MaterialPipelineKey = uint32_t;

[[nodiscard]] MaterialPipelineKey makeMaterialPipelineKey(
    const resource::MaterialData& data);

inline uint32_t uvSetFromKey(MaterialPipelineKey key, MaterialUvSlot slot) {
    return (key >> static_cast<uint32_t>(slot)) & 1u;
}

} // namespace render