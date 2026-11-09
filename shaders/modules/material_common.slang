module material_common;

// --- Material texture slots -------------------------------------------------
// The INDEX IS THE SLOT.  0..4 follow resource::MaterialTextureSlot
// (src/resource/gltf_importer.hpp), which is glTF's field order:
//   baseColor -> metallicRoughness -> normal -> occlusion -> emissive.
//
// The array size must equal kMaterialSlotCount; the element count is checked
// against the descriptor set layout at startup.
public static const int kBaseColorSlot = 0;
public static const int kMetallicRoughnessSlot = 1;
public static const int kNormalSlot = 2;
public static const int kOcclusionSlot = 3;
public static const int kEmissiveSlot = 4;
public static const int kMaterialSlotCount = 5;

[[vk::binding(0, 1)]] public Texture2D maps[kMaterialSlotCount];
[[vk::binding(1, 1)]] public SamplerState samplers[kMaterialSlotCount];

// Push constant
public struct PushConstants {
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float alphaMaskCutoff; // Alpha cutoff threshold
    float clearcoatFactor; // KHR_materials_clearcoat
    float clearcoatRoughness;
    uint flags; // Material properites
};
[[vk::push_constant]] public ConstantBuffer<PushConstants> material;