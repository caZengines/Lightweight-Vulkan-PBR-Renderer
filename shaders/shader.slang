import common;
struct VertexInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct VertexOutput {
    float4 pos : SV_POSITION;

    float2 fragTexCoord;
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
[[vk::binding(0, 1)]] Texture2D baseColorMap;
[[vk::binding(1, 1)]] SamplerState baseColorSampler;
[[vk::binding(2, 1)]] Texture2D metallicRoughnessMap;
[[vk::binding(3, 1)]] SamplerState mrSampler;
[[vk::binding(4, 1)]] Texture2D normalMap;
[[vk::binding(5, 1)]] SamplerState normalSampler;
[[vk::binding(6, 1)]] Texture2D occlusionMap;
[[vk::binding(7, 1)]] SamplerState occlusionSampler;
[[vk::binding(8, 1)]] Texture2D emissiveMap;
[[vk::binding(9, 1)]] SamplerState emissiveSampler;

// Push constant: per-material render flags (bitmask)
struct PushConstants {
    uint32_t flags;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

[shader("vertex")]
VertexOutput vertMain(VertexInput Input, InstanceData instance) {
    VertexOutput Output;

    float4x4 instanceModel = instance.model;

    float4 worldPosition = mul(instanceModel, float4(Input.inPosition, 1.0));

    Output.pos = mul(ubo.proj, mul(ubo.view, worldPosition));
    Output.fragTexCoord = Input.inTexCoord;
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

static float constant = 1.0; 
static float linear = 0.09;     // Linear attenuation coefficient
static float quadratic = 0.032; // Square attenuation coefficient

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    // Bitmask albedo sampling
    float4 albedo;
    albedo = baseColorMap.Sample(baseColorSampler, inVert.fragTexCoord);

    // Bitmask normal sampling
    float3 N;
    float3 tn = normalMap.Sample(normalSampler, inVert.fragTexCoord).rgb;
    tn.rg = tn.rg * 2 - 1.0;
    tn.g = tn.g;
    N = normalize(tn);

    float3 L = normalize(inVert.fragTangentLightDir);
    float3 V = normalize(inVert.fragTangentViewDir);
    float3 halfwayDir = normalize(L + V);

     // dot light
     float dist = length(ubo.light.position.xyz - inVert.worldPos);
     float atten = 1.0 / (constant + linear * dist + quadratic * (dist * dist));
     float3 radiance = ubo.light.color.rgb * ubo.light.intensity * atten;

     // Ambient
     float3 ambient = albedo.rgb * 0.1;

     // Diffuse(lambert)
     float diff = max(dot(N, L), 0.0);
     float3 diffuse = albedo.rgb * diff;

     // Specular
     float shininess = 32.0;
     float spec = max(dot(N, halfwayDir), 0.0);
     float3 specular = pow(spec, shininess);

     float3 finalColor = albedo.xyz;
    // float2 uv = inVert.fragTexCoord * 50;
    // uv.x = uv.x - (int)uv.x;
    // uv.y = uv.y - (int)uv.y;
    // uv = abs(uv - 0.5);
    // float mincoord = min(uv.x, uv.y);
    // float isShader = 1 - step(0.05, mincoord);
    // float3 debugColor = sin(inVert.worldPos * 0.5) * 0.5 + 0.5;
    // float3 finalColor = debugColor * isShader;

    return float4(finalColor, albedo.a);
}
