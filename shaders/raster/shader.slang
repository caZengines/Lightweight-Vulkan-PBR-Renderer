import common;
import brdf;

struct VertexInput {
    float3 inPosition;
    float2 inTexCoord : TEXCOORD0;
    float3 inNormal;
    float4 inTangent;
};

struct VertexOutput {
    float4 pos : SV_POSITION;

    float2 UV : TEXCOORD0;
    float3 fragTangentLightDir;
    float3 fragTangentViewDir;
    float3 worldPos;
};

struct InstanceData {
    float4x4 model;
};

struct Light {
    float4 position;
    float4 color;
    float intensity;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;
    Light light;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformBuffer> ubo;

// --- Material texture slots -------------------------------------------------
// The INDEX IS THE SLOT.  0..4 follow resource::MaterialTextureSlot
// (src/resource/gltf_importer.hpp), which is glTF's field order:
//   baseColor -> metallicRoughness -> normal -> occlusion -> emissive.
//
// The array size must equal kMaterialSlotCount; the element count is checked
// against the descriptor set layout at startup.
static const int kBaseColorSlot         = 0;
static const int kMetallicRoughnessSlot = 1;
static const int kNormalSlot            = 2;
static const int kOcclusionSlot         = 3;
static const int kEmissiveSlot          = 4;
static const int kMaterialSlotCount     = 5;

[[vk::binding(0, 1)]] Texture2D    maps[kMaterialSlotCount];
[[vk::binding(1, 1)]] SamplerState samplers[kMaterialSlotCount];

// Push constant
struct PushConstants {
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaMask;       // Alpha masking enable flag
    float alphaMaskCutoff; // Alpha cutoff threshold
};
[[vk::push_constant]] ConstantBuffer<PushConstants> material;

[shader("vertex")]
VertexOutput vertMain(VertexInput Input, InstanceData instance) {
    VertexOutput Output;

    float4x4 instanceModel = instance.model;

    float4 worldPosition = mul(instanceModel, float4(Input.inPosition, 1.0));

    Output.pos = mul(ubo.proj, mul(ubo.view, worldPosition));
    Output.UV = Input.inTexCoord;
    Output.worldPos = worldPosition.xyz;

    //create TBN Matrix
    float3 N = normalize(mul((float3x3)instanceModel, Input.inNormal));
    float3 T = normalize(mul((float3x3)instanceModel, Input.inTangent.xyz));
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T) * Input.inTangent.w;

    float3 worldLightDir = ubo.light.position.xyz - worldPosition.xyz;
    float3 worldViewDir = ubo.cameraPos.xyz - worldPosition.xyz;
    //Transfer World Space to Tangent Space
    Output.fragTangentLightDir = float3(
        dot(worldLightDir, T),
        dot(worldLightDir, B),
        dot(worldLightDir, N)
    );
    Output.fragTangentViewDir = float3(
        dot(worldViewDir, T),
        dot(worldViewDir, B),
        dot(worldViewDir, N)
    );
    
    return Output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    // === MATERIAL PROPERTY SAMPLING ===
    // Sample base color texture and apply material color factor
    float4 baseColor;
    baseColor = maps[kBaseColorSlot].Sample(samplers[kBaseColorSlot], inVert.UV) * material.baseColorFactor;

    // Sample metallic-roughness texture (metallic=B channel, roughness=G channel)
    float2 metallicRoughness = maps[kMetallicRoughnessSlot].Sample(samplers[kMetallicRoughnessSlot], inVert.UV).bg;
    float metallic = metallicRoughness.x * material.metallicFactor;
    float roughness = metallicRoughness.y * material.roughnessFactor;

    // Sample ambient occlusion (typically stored in red channel)
    float ao = maps[kOcclusionSlot].Sample(samplers[kOcclusionSlot], inVert.UV).r;
    // Sample emissive texture for self-illuminating materials
    float3 emissive = maps[kEmissiveSlot].Sample(samplers[kEmissiveSlot], inVert.UV).rgb;

    float3 N;
    float3 tn = maps[kNormalSlot].Sample(samplers[kNormalSlot], inVert.UV).rgb;
    tn.rgb = tn.rgb * 2 - 1.0;
    N = normalize(tn);

    // === PBR MATERIAL SETUP ===
    // Non-metals: low reflectance (~0.04), Metals: colored reflectance from base color
    float3 f0 = float3(0.04, 0.04, 0.04);
    f0 = lerp(f0, baseColor.rgb, metallic);

    // Initialize outgoing radiance accumulator
    float3 Lo = float3(0.0, 0.0, 0.0);

    // Calculate light direction and attenuation
    float3 L = normalize(inVert.fragTangentLightDir);
    float3 V = normalize(inVert.fragTangentViewDir);
    float3 H = normalize(L + V);
    float distance = length(inVert.fragTangentLightDir);
    float attenuation = 1.0 / (distance * distance); // Inverse square falloff
    float3 radiance = ubo.light.color.rgb * ubo.light.intensity * attenuation;

    // === BRDF EVALUATION ===
    float NdotL = max(dot(N, L), 0.0); // Lambertian falloff
    float NdotV = max(dot(N, V), 0.0); // View angle
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0); // For Fresnel calculation

    // Evaluate Cook-Torrance BRDF components
    float D = DistributionGGX(NdotH, roughness); // Normal distribution
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(HdotV, f0);

    // Calculate specular BRDF
    float3 numerator = D * G * F;
    float denominator = 4 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;

    Lo += (kD * diffuseBRDF(baseColor.rgb) + specular) * radiance * NdotL;

    // Add simple ambient lighting (should be replaced with IBL in production)
    float3 ambient = float3(0.03, 0.03, 0.03) * baseColor.rgb * ao;

    float3 color = emissive * material.emissiveFactor.rgb + Lo + ambient;

    // === HDR TONE MAPPING AND GAMMA CORRECTION ===
    // Apply Reinhard tone mapping to compress HDR values to [0,1] range
    color = color / (color + float3(1.0, 1.0, 1.0));

    return float4(color, baseColor.a);
}
