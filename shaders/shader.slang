struct VertexInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct InstanceData {
    float4x4 model;
    uint32_t enableNormal;
    uint32_t materialID;
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

struct VertexOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float3 fragNorCoord;
    float3 fragTangentLightDir;
    float3 fragTangentViewDir;
    float3 worldPos;

    uint32_t isEnableNormalTexture;
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

    Output.isEnableNormalTexture = instance.enableNormal;
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

static float constant = 1.0;    // 常数项，防止分母过小
static float linear = 0.09;     // 线性衰减系数
static float quadratic = 0.032; // 平方衰减系数

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    //Sample texture
    float4 albedo = albedoTexture.Sample(gtexSampler, inVert.fragTexCoord);
    // Sample normal
    float3 N;
    if (inVert.isEnableNormalTexture > 0) {
        float3 tn = normalTexture.Sample(gNorSampler, inVert.fragTexCoord).rgb;
        tn.rg = tn.rg * 2 - 1.0;
        tn.g = 1.0 - tn.g;
        N = normalize(tn);
    }
    else {
        N = inVert.fragNorCoord;
    }

    float3 L = normalize(inVert.fragTangentLightDir);
    float3 V = normalize(inVert.fragTangentViewDir);
    float3 halfwayDir = normalize(L + V);

    // dot light
    float dist = length(ubo.light.position.xyz - inVert.worldPos);
    float atten = 1.0 / (constant + linear * dist + quadratic * (dist * dist));
    float3 radiance = ubo.light.color.rgb * ubo.light.intensity * atten;

    // Ambient
    float3 ambient = albedo.rgb * 0.07;

    // Diffuse(lambert)
    float diff = max(dot(N, L), 0.0);
    float3 diffuse = albedo.rgb * diff;

    // Specular
    float shininess = 32.0;
    float spec = max(dot(N, halfwayDir), 0.0);
    float3 specular = pow(spec, shininess);

    float3 finalColor = (ambient + diffuse + specular) * radiance;

    return float4(ambient, albedo.a);
}
