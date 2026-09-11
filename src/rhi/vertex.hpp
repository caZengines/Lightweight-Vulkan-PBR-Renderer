#pragma once

#include <cstdint>
#include <functional>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace rhi {

// Pure vertex/instance data. The Vulkan binding/attribute descriptions that
// used to live here moved to render/pipeline.cpp
// these structs must stay GPU-free so the scene layer can
// use them.
struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
    int8_t normal[4];
    int8_t tangent[4];
    // Second UV set (glTF TEXCOORD_1). Loaded since the glTF scene importer;
    // NOT yet exposed to the shader — when the material work wires it up,
    // add the vertex input at location 4 and shift the instance matrix
    // attributes to locations 5-8 in render/pipeline.cpp.
    glm::vec2 texCoord1;

    void setNormal(const glm::vec3& n);
    void setTangent(const glm::vec3& tangent_, const float& handedness_);

    // Dedup key: every attribute that postProcess does not regenerate.
    // Tangents are included so file-provided tangents partake in identity;
    // when tangents are computed later they are all zero here, so dedup
    // behaves exactly as before.
    bool operator==(const Vertex& other) const {
        return pos == other.pos && texCoord == other.texCoord &&
               texCoord1 == other.texCoord1 &&
               normal[0] == other.normal[0] && normal[1] == other.normal[1] &&
               normal[2] == other.normal[2] &&
               tangent[0] == other.tangent[0] && tangent[1] == other.tangent[1] &&
               tangent[2] == other.tangent[2] && tangent[3] == other.tangent[3];
    }
};

struct InstanceData {
    glm::mat4 model;
};
static_assert(sizeof(InstanceData) == 64, "InstanceData must match shader layout");

}

namespace std {
    template<> struct hash<rhi::Vertex>{
        size_t operator()(rhi::Vertex const& v) const {
            size_t h1 = hash<glm::vec3>{}(v.pos);
            size_t h2 = hash<glm::vec2>{}(v.texCoord);
            size_t h4 = hash<glm::vec2>{}(v.texCoord1);
            size_t h3 = hash<uint8_t>{}(v.normal[0]) ^ (hash<uint8_t>{}(v.normal[1]) << 8) ^ (hash<uint8_t>{}(v.normal[2]) << 16);
            size_t h5 = hash<uint8_t>{}(v.tangent[0]) ^ (hash<uint8_t>{}(v.tangent[1]) << 8) ^
                        (hash<uint8_t>{}(v.tangent[2]) << 16) ^ (hash<uint8_t>{}(v.tangent[3]) << 24);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };
}