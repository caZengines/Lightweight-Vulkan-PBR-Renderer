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
    void setTangent(const glm::vec3& deltaPos1_, const glm::vec3& deltaPos2_, const glm::vec2& deltaUV1_, const glm::vec2& deltaUV2_, const glm::vec3& n);

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescription();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && texCoord == other.texCoord && normal[0] == other.normal[0] && normal[1] == other.normal[1] && normal[2] == other.normal[2]
            && tangent[0] == other.tangent[0] && tangent[1] == other.tangent[1] && tangent[2] == other.tangent[2];
    }
};
namespace std{
    template<> struct hash<Vertex>{
        size_t operator()(Vertex const& vertex) const {
            return (((hash<glm::vec3>()(vertex.pos) ^ (hash<uint8_t>()(vertex.normal[0]) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1));
        }
    };
}

struct InstanceData {
    glm::mat4 model;

    static vk::VertexInputBindingDescription getBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescription();
};
static_assert(sizeof(InstanceData) == 64, "InstanceData must match shader layout");