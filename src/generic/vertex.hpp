#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
    int8_t normal[4];
    int8_t tangent[4];

    void setNormal(const glm::vec3& n);
    void setTangent(const glm::vec3& tangent_, const float& handedness_);

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescription();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && texCoord == other.texCoord && normal[0] == other.normal[0] && normal[1] == other.normal[1] && normal[2] == other.normal[2];
    }
};
namespace std {
    template<> struct hash<Vertex>{
        size_t operator()(Vertex const& v) const {
            size_t h1 = hash<glm::vec3>{}(v.pos);
            size_t h2 = hash<glm::vec2>{}(v.texCoord);
            size_t h3 = hash<uint8_t>{}(v.normal[0]) ^ (hash<uint8_t>{}(v.normal[1]) << 8) ^ (hash<uint8_t>{}(v.normal[2]) << 16);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

struct InstanceData {
    glm::mat4 model;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescription();
};
static_assert(sizeof(InstanceData) == 64, "InstanceData must match shader layout");