#include "generic/vertex.hpp"

void Vertex::setNormal(const glm::vec3& n){
    normal[0] = static_cast<int8_t>(glm::round(glm::clamp(n.x, -1.0f, 1.0f) * 127.0f));
    normal[1] = static_cast<int8_t>(glm::round(glm::clamp(n.y, -1.0f, 1.0f) * 127.0f));
    normal[2] = static_cast<int8_t>(glm::round(glm::clamp(n.z, -1.0f, 1.0f) * 127.0f));
    normal[3] = 0;
}

void Vertex::setTangent(const glm::vec3& deltaPos1_, const glm::vec3& deltaPos2_, const glm::vec2& deltaUV1_, const glm::vec2& deltaUV2_, const glm::vec3& n){
    float r = 1.0f / (deltaUV1_.x * deltaUV2_.y - deltaUV1_.y * deltaUV2_.x);
    glm::vec3 tan = (deltaPos1_ * deltaUV2_.y - deltaPos2_ * deltaUV1_.y) * r;
    tan = glm::normalize(tan - n * glm::dot(tan, n));

    glm::vec3 bitan = (deltaPos2_ * deltaUV1_.x - deltaPos1_ * deltaUV2_.x) * r;
    int8_t handedness = (glm::dot(glm::cross(n, tan), bitan) >= 0.0f) ? 1 : -1;

    tangent[0] = static_cast<int8_t>(glm::round(glm::clamp(tan.x, -1.0f, 1.0f) * 127.0f));
    tangent[1] = static_cast<int8_t>(glm::round(glm::clamp(tan.y, -1.0f, 1.0f) * 127.0f));
    tangent[2] = static_cast<int8_t>(glm::round(glm::clamp(tan.z, -1.0f, 1.0f) * 127.0f));
    tangent[3] = static_cast<int8_t>(handedness * 127);
}

vk::VertexInputBindingDescription Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription description;
    description.setBinding(0).setStride(sizeof(Vertex)).setInputRate(vk::VertexInputRate::eVertex);

    return description;
}

std::array<vk::VertexInputAttributeDescription, 4> Vertex::getAttributeDescription() {
    vk::VertexInputAttributeDescription posAttribute;
    posAttribute.setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(offsetof(Vertex, pos));
    vk::VertexInputAttributeDescription uvAttribute;
    uvAttribute.setLocation(1).setFormat(vk::Format::eR32G32Sfloat).setOffset(offsetof(Vertex, texCoord));
    vk::VertexInputAttributeDescription norAttribute;
    norAttribute.setLocation(2).setFormat(vk::Format::eR8G8B8A8Snorm).setOffset(offsetof(Vertex, normal));
    vk::VertexInputAttributeDescription tanAttribute;
    tanAttribute.setLocation(3).setFormat(vk::Format::eR8G8B8A8Snorm).setOffset(offsetof(Vertex, tangent));

    return {posAttribute, uvAttribute, norAttribute, tanAttribute};
}