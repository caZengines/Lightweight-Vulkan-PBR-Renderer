module brdf;

import common;

// Normal Distribution Function (D) - GGX/Trowbridge-Reitz Distribution
public float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1) + 1;
    denom = PI * denom * denom;
    return a2 / denom; 
}


internal float GeometrySchlickGGX(float NdotV, float k) {
    float denom = NdotV * (1 - k) + k;
    return NdotV / denom;
}
// Geometry Function (G) - Smith's method with Schlick-GGX approximation
public float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    // Geometry obstruction from view direction (masking)
    float GGX1 = GeometrySchlickGGX(NdotV, k);
    // Geometry obstruction from light direction (shadowing)
    float GGX2 = GeometrySchlickGGX(NdotL, k);

    return GGX1 * GGX2;
}

// Fresnel Reflectance (F) - Schlick's approximation
public float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1 - F0) * pow((1 - cosTheta), 5.0);
}

public float3 diffuseBRDF(float3 diffuseColor) {
    return diffuseColor / PI;
}