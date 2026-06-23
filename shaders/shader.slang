struct VertexInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 inNormal;
    float4 inTangent;
};

struct Light {
    float4 position;
    float4 color;
    float intensity;
}

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;

    float4 cameraPos;
    Light light;
};
[[vk::binding(0, 0)]]ConstantBuffer<UniformBuffer> ubo;

struct VertexOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float3 fragTangentLightDir;
    float3 fragTangentViewDir;
    float3 worldPos;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput Input) {
    VertexOutput Output;

    float4 worldPosition = mul(ubo.model, float4(Input.inPosition, 1.0));

    Output.pos = mul(ubo.proj, mul(ubo.view, worldPosition));
    Output.fragTexCoord = Input.inTexCoord;
    Output.worldPos = worldPosition.xyz;
    //create TBN Matrix
    float3 N = normalize(mul((float3x3)ubo.model, Input.inNormal));
    float3 T = normalize(mul((float3x3)ubo.model, Input.inTangent.xyz));
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

[[vk::binding(0, 1)]]Sampler2D Texture;
[[vk::binding(1, 1)]]Sampler2D normalTexture;

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target {
    //Sample texture
    float4 albedo = Texture.Sample(inVert.fragTexCoord);
    // Sample normal
    float3 tn = normalTexture.Sample(inVert.fragTexCoord).rgb;
    tn.rg = tn.rg * 2 - 1.0;
    tn.g = 1.0 - tn.g;
    float3 N = normalize(tn);

    float3 L = normalize(inVert.fragTangentLightDir);
    float3 V = normalize(inVert.fragTangentViewDir);
    float3 halfwayDir = normalize(L + V);

    // dot light
    float dist = length(ubo.light.position.xyz - inVert.worldPos);
    float atten = 0.8;
    float3 radiance = ubo.light.color.rgb * ubo.light.intensity * atten;

    // Ambient
    float3 ambient = 0.05 * albedo.rgb;

    // Diffuse(lambert)
    float diff = max(dot(N, L), 0.0);
    float3 diffuse = albedo.rgb * diff;

    // Specular
    float shininess = 32.0;
    float spec = max(dot(N, halfwayDir), 0.0);
    float3 specular = pow(spec, shininess);

    float3 finalColor = (ambient + diffuse + specular) * radiance;

    return float4(finalColor, albedo.a);
}
