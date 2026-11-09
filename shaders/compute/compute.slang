#language slang 2026
struct Particle {
    float4 position;
    float4 velocity;
    float4 color;
};


[[vk::binding(0, 2)]] StructuredBuffer<Particle> particleIn;
[[vk::binding(1, 2)]] RWStructuredBuffer<Particle> particleOut;

struct SimParam {
    float dt;
    float gravity;
}

//[[vk::push_constant]] ConstantBuffer<SimParam> simParam;

//static const float groundY = 0.0;
//static const float restitution = 0.6;

static const float a = 0.95;
static const float b = 0.7;
static const float c = 0.6;
static const float d = 3.5;
static const float e = 0.25;
static const float f = 0.1;

float pow3(float sum) {
    return sum * sum * sum;
}

float3 aizawa(float3 p) {
    float sub = p.z - b;
    float mulxy = p.x * p.x + p.y * p.y;
    return float3(
        sub * p.x - d * p.y,
        d * p.x + sub * p.y,
        c + a * p.z - (pow3(p.z) / 3.0) - mulxy * (1.0 + e * p.z) + f * p.z * pow3(p.x)
    );
}

[shader("compute")]
[numthreads(256, 1, 1)]
void csMain(uint3 id: SV_DispatchThreadID) {
    uint i = id.x;
    float3 p = particleIn[i].position.xyz;

    const float dt = 1.0 / 1000.0;
    float3 k1 = aizawa(p);
    float3 k2 = aizawa(p + 0.5 * dt * k1);
    float3 k3 = aizawa(p + 0.5 * dt * k2);
    float3 k4 = aizawa(p + dt * k3);
    float3 dp = (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
    float3 pnew = p + dp * dt;
    if (any(isnan(pnew)) || any(isinf(pnew)) || length(pnew) > 1e3) {
        pnew = float3(0.1, 0.0, 0.0);
    }

    particleOut[i].position = float4(pnew, 1.0);
    particleOut[i].velocity = float4(dp, 0.0);

    const float t = saturate(length(dp) / 4.0);
    particleOut[i].color = lerp(
        float4(0.2, 0.4, 1.0, 1.0),
        float4(1.0, 0.1, 0.26, 1.0),
        t);
}
