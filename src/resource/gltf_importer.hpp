#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "resource/mesh_data.hpp"

namespace resource {

enum class AlphaMode { Opaque, Mask, Blend };

// Material texture slots in glTF field order
// (pbrMetallicRoughness → normal → occlusion → emissive).
enum class MaterialTextureSlot : uint32_t {
    BaseColor = 0,
    MetallicRoughness,
    Normal,
    Occlusion,
    Emissive,
    Count,
};

// One texture reference inside a material.
struct TextureSlot {
    int32_t  texture  = -1;   // index into GltfScene::textures; -1 = none
    uint32_t texCoord = 0;    // UV set index (TEXCOORD_${texCoord})
    float    scale    = 1.0f; // normal: scale; occlusion: strength; ignored elsewhere
};

// glTF material: factors plus per-slot texture references (pure CPU data).
struct MaterialData {
    glm::vec4 baseColorFactor{1.0f};
    float     metallic  = 1.0f;
    float     roughness = 1.0f;
    glm::vec3 emissiveFactor{0.0f};
    float     normalScale       = 1.0f;
    float     occlusionStrength = 1.0f;
    float     alphaCutoff       = 0.5f;
    AlphaMode alphaMode         = AlphaMode::Opaque;
    bool      doubleSided       = false;
    std::array<TextureSlot, size_t(MaterialTextureSlot::Count)> slots;
};

// glTF sampler filter/wrap enum values, passed through verbatim (the sampler
// cache maps them to Vulkan).
struct SamplerParams {
    static constexpr int32_t kWrapRepeat = 10497;  // glTF default
    int32_t minFilter = -1;  // -1 = unspecified
    int32_t magFilter = -1;
    int32_t wrapS = kWrapRepeat;
    int32_t wrapT = kWrapRepeat;
};

// Where one texture's encoded pixels come from. Decoding is the caller's job
// (TextureImporter), so pixels stay encoded here.
struct TextureSource {
    bool                 embedded = false; // true: bytes; false: uri on disk
    std::string          uri;              // absolute path of an external image
    std::vector<uint8_t> bytes;            // encoded image data for embedded images
    std::string          mime;             // mime type of an embedded image
    int32_t              image = -1;       // glTF image index
    SamplerParams        sampler;
    bool                 valid = false;    // false: unusable entry, treated as "no texture"
};

// One drawable glTF primitive: its local-space geometry, the baked world
// transform of the node chain that referenced it, and the glTF material index
// it points at (0xFFFFFFFF when the primitive has no material).
//
// The world matrix rides OUTSIDE the mesh: vertices stay in mesh-local space
// so the same geometry can be re-placed or instanced freely.
struct GltfPrimitive {
    MeshData    mesh;
    glm::mat4   world         = glm::mat4(1.0f);
    uint32_t    materialIndex = 0xFFFFFFFFu;  // glTF material index; ~0u = none
    std::string name;                         // owning glTF mesh name (debugging)
};

// CPU-side result of a full glTF 2.0 scene import — pure data, no Vulkan.
struct GltfScene {
    std::vector<GltfPrimitive> primitives;
    std::vector<MaterialData>  materials;   // materialIndex → entry
    std::vector<TextureSource> textures;    // glTF texture index → entry (gaps possible)
};

class GltfSceneImporter {
    public:
        // Full-scene glTF import (.gltf with external .bin, or .glb):
        //   * one GltfPrimitive per triangle primitive, node hierarchy
        //     collapsed into a per-primitive world matrix
        //   * POSITION / NORMAL / TANGENT (vec4 float) / TEXCOORD_0 / _1
        //   * materials parsed to MaterialData; every texture referenced by a
        //     material materialized as a TextureSource (URI resolved relative
        //     to the glTF file, sampler params dereferenced)
        //   * sparse accessors, strips/fans, 8/16/32-bit indices
        // UV convention: glTF V runs top-down, which matches stb's row order
        // and this project's upload path — UVs are passed through UNFLIPPED
        // (the OBJ loader flips because OBJ's V runs bottom-up).
        // Unsupported (warned + skipped): non-triangle modes, primitives
        // without POSITION, TEXCOORD_2+, KHR_materials_* extensions, data URIs.
        // Throws on parse failure or when no drawable geometry remains.
        static GltfScene load(const std::string& path);
};

}  // namespace resource
