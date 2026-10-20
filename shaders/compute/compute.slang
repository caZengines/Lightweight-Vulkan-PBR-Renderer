struct Particle {
    float4 position;
    float4 velocity;
};

struct ParticleSSBO {
    Particle particles;
};

[[vk::binding(0, 2)]] StructuredBuffer<ParticleSSBO> particleIn;
[[vk::binding(1, 2)]] RWStructuredBuffer<ParticleSSBO> particleOut;


[shader("compute")]
[numthreads(1, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID) {
    // Compute pipeline creation and dispatch-recording smoke; no output this round.
}
