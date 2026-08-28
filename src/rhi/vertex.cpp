#include "rhi/vertex.hpp"

namespace rhi {



void Vertex::setNormal(const glm::vec3& n){
    normal[0] = static_cast<int8_t>(glm::round(glm::clamp(n.x, -1.0f, 1.0f) * 127.0f));
    normal[1] = static_cast<int8_t>(glm::round(glm::clamp(n.y, -1.0f, 1.0f) * 127.0f));
    normal[2] = static_cast<int8_t>(glm::round(glm::clamp(n.z, -1.0f, 1.0f) * 127.0f));
    normal[3] = 0;
}

void Vertex::setTangent(const glm::vec3& tangent_, const float& handedness_){
    tangent[0] = static_cast<int8_t>(glm::round(glm::clamp(tangent_.x, -1.0f, 1.0f) * 127.0f));
    tangent[1] = static_cast<int8_t>(glm::round(glm::clamp(tangent_.y, -1.0f, 1.0f) * 127.0f));
    tangent[2] = static_cast<int8_t>(glm::round(glm::clamp(tangent_.z, -1.0f, 1.0f) * 127.0f));
    tangent[3] = static_cast<int8_t>(handedness_ * 127);
}

}
