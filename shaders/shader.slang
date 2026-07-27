struct VertexInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct InstanceData {
    float4x4 model;
};

// Push constant: per-material render flags (bitmask)
struct PushConstants {
    uint32_t flags;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

static const uint32_t FLAG_ALBEDO_TEXTURE = 1u << 0; // bit 0
static const uint32_t FLAG_NORMAL_TEXTURE = 1u << 1; // bit 1

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

struct VertexOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float3 fragNorCoord;
    float3 fragTangentLightDir;
    float3 fragTangentViewDir;
    float3 worldPos;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput Input, InstanceData instance) {
    VertexOutput Output;

    float4x4 instanceModel = instance.model;

    float4 worldPosition = mul(instanceModel, float4(Input.inPosition, 1.0));

    Output.pos = mul(ubo.proj, mul(ubo.view, worldPosition));
    Output.fragTexCoord = Input.inTexCoord;
    Output.fragNorCoord = Input.inNormal;
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

[[vk::binding(0, 1)]] Texture2D albedoTexture;
[[vk::binding(1, 1)]] Texture2D normalTexture;
[[vk::binding(2, 1)]] SamplerState gtexSampler;
[[vk::binding(3, 1)]] SamplerState gNorSampler;

static float constant = 1.0; 
static float linear = 0.09;     // Linear attenuation coefficient
static float quadratic = 0.032; // Square attenuation coefficient

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    // Bitmask albedo sampling
    float4 albedo;
    if ((pc.flags & FLAG_ALBEDO_TEXTURE) != 0u) {
        albedo = albedoTexture.Sample(gtexSampler, inVert.fragTexCoord);
    } else {
        albedo = float4(0.8f, 0.8f, 0.8f, 1.0f); // solid gray fallback
    }

    // Bitmask normal sampling
    float3 N;
    if ((pc.flags & FLAG_NORMAL_TEXTURE) != 0u) {
        float3 tn = normalTexture.Sample(gNorSampler, inVert.fragTexCoord).rgb;
        tn.rg = tn.rg * 2 - 1.0;
        tn.g = 1.0 - tn.g;
        N = normalize(tn);
    } else {
        N = normalize(inVert.fragNorCoord);
    }

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

     float3 finalColor = (ambient + diffuse + specular) * radiance;
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
