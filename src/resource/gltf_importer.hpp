#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "resource/mesh_data.hpp"

namespace resource {

// One drawable glTF primitive: its local-space geometry, the baked world
// transform of the node chain that referenced it, and the glTF material index
// it points at (0xFFFFFFFF when the primitive has no material).
//
// The world matrix rides OUTSIDE the mesh: vertices stay in mesh-local space
// so the same geometry can be instanced/referenced later (and, at some point,
// per-instance transformed by the ray-tracing TLAS). The app layer puts the
// matrix into a SceneObject instance placement.
struct GltfPrimitive {
    MeshData    mesh;
    glm::mat4   world         = glm::mat4(1.0f);
    uint32_t    materialIndex = 0xFFFFFFFFu;  // glTF material index; ~0u = none
    std::string name;                         // owning glTF mesh name (debugging)
};

// CPU-side result of a full glTF 2.0 scene import — pure data.
// Textures/images are NOT decoded here; the GPU-side material work is the
// app layer's job (see AGENTS.md module map, resource/ + app/).
struct GltfScene {
    std::vector<GltfPrimitive> primitives;
};

class GltfSceneImporter {
    public:
        // Full-scene glTF import (.gltf with external .bin, or .glb):
        //   * every triangle primitive becomes one GltfPrimitive (no merging —
        //     one material per primitive is what the render path needs)
        //   * the node hierarchy is collapsed: each primitive carries the
        //     product of its parents' matrix/TRS transforms
        //   * POSITION / NORMAL / TANGENT / TEXCOORD_0 / TEXCOORD_1 attributes
        //   * sparse accessors, strips/fans, 8/16/32-bit indices as in
        //     MeshImporter (the accessor machinery lives in the .cpp)
        // UV convention: glTF V runs top-down, which matches stb's row order
        // and this project's upload path — UVs are passed through UNFLIPPED
        // (the OBJ loader flips because OBJ's V runs bottom-up).
        // Unsupported (warned + skipped): non-triangle modes, primitives
        // without POSITION, TEXCOORD_2+. Throws on parse failure or when no
        // drawable geometry remains.
        static GltfScene load(const std::string& path);
};

}  // namespace resource
