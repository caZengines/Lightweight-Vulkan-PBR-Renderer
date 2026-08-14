#include "generic/glTFloader.hpp"

#include "extern/tiny_gltf_v3.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

// =============================================================================
// glTF 2.0 model loading through the tinygltf v3 C API.
//
// Scope (basic geometry):
//   * .gltf (with external .bin buffers) and .glb files
//   * POSITION (vec3 float), NORMAL (vec3 float or normalized int) and
//     TEXCOORD_0 (vec2 float or normalized int) attributes
//   * TRIANGLES / TRIANGLE_STRIP / TRIANGLE_FAN primitives (strips and fans
//     are expanded to a triangle list); point/line modes are skipped
//   * unsigned byte/short/int index accessors, or none (non-indexed)
//
// Not supported yet: sparse accessors, Draco compression, morph targets,
// materials/textures, node transforms (warned and skipped/filled with
// defaults). Missing normals are regenerated as smooth normals and tangents
// are computed by the Mesh constructor.
// =============================================================================

namespace {

const char* severityName(tg3_severity severity) {
    switch (severity) {
        case TG3_SEVERITY_INFO:    return "INFO";
        case TG3_SEVERITY_WARNING: return "WARNING";
        case TG3_SEVERITY_ERROR:   return "ERROR";
        default:                   return "UNKNOWN";
    }
}

// Find a primitive attribute ("POSITION", "NORMAL", "TEXCOORD_0", ...) and
// return its accessor index, or -1 when the attribute is absent.
int32_t findAttribute(const tg3_primitive* primitive, const char* name) {
    for (uint32_t k = 0; k < primitive->attributes_count; ++k) {
        if (tg3_str_equals_cstr(primitive->attributes[k].key, name)) {
            return primitive->attributes[k].value;
        }
    }
    return -1;
}

// Bounds-checked pointer to the element at the given index inside an accessor.
const uint8_t* accessorElementPtr(const tg3_accessor& accessor,
                                  const tg3_buffer_view& view,
                                  const tg3_buffer& buffer,
                                  uint64_t index) {
    const int32_t componentSize  = tg3_component_size(accessor.component_type);
    const int32_t componentCount = tg3_num_components(accessor.type);
    const int32_t stride         = tg3_accessor_byte_stride(&accessor, &view);
    if (componentSize <= 0 || componentCount <= 0 || stride <= 0) {
        throw std::runtime_error("glTF: accessor with unsupported component type or type");
    }
    if (index >= accessor.count) {
        throw std::runtime_error("glTF: accessor element index out of range");
    }
    const uint64_t elementSize =
        static_cast<uint64_t>(componentSize) * static_cast<uint64_t>(componentCount);
    const uint64_t offset =
        view.byte_offset + accessor.byte_offset + index * static_cast<uint64_t>(stride);
    if (offset + elementSize > view.byte_offset + view.byte_length ||
        offset + elementSize > buffer.data.count) {
        throw std::runtime_error("glTF: accessor data exceeds its buffer view / buffer");
    }
    return buffer.data.data + offset;
}

// Attribute accessors we can read: FLOAT / DOUBLE, or normalized integer types.
bool attributeReadable(const tg3_accessor& accessor) {
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_FLOAT:
        case TG3_COMPONENT_TYPE_DOUBLE:
            return true;
        case TG3_COMPONENT_TYPE_BYTE:
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TG3_COMPONENT_TYPE_SHORT:
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            return accessor.normalized != 0;
        default:
            return false;
    }
}

// Read one attribute element as a float4. The accessor must have been
// accepted by attributeReadable().
glm::vec4 readAttributeElement(const tg3_accessor& accessor,
                               const tg3_buffer_view& view,
                               const tg3_buffer& buffer,
                               uint64_t index) {
    const uint8_t* ptr = accessorElementPtr(accessor, view, buffer, index);
    const int32_t components = tg3_num_components(accessor.type);
    glm::vec4 result(0.0f);
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_FLOAT:
            for (int32_t c = 0; c < components; ++c) {
                float value;
                std::memcpy(&value, ptr + c * sizeof(float), sizeof(value));
                result[c] = value;
            }
            break;
        case TG3_COMPONENT_TYPE_DOUBLE:
            for (int32_t c = 0; c < components; ++c) {
                double value;
                std::memcpy(&value, ptr + c * sizeof(double), sizeof(value));
                result[c] = static_cast<float>(value);
            }
            break;
        case TG3_COMPONENT_TYPE_BYTE:
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TG3_COMPONENT_TYPE_SHORT:
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (int32_t c = 0; c < components; ++c) {
                switch (accessor.component_type) {
                    case TG3_COMPONENT_TYPE_BYTE: {
                        int8_t value;
                        std::memcpy(&value, ptr + c * sizeof(int8_t), sizeof(value));
                        result[c] = std::max(static_cast<float>(value) / 127.0f, -1.0f);
                        break;
                    }
                    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                        result[c] = static_cast<float>(ptr[c]) / 255.0f;
                        break;
                    case TG3_COMPONENT_TYPE_SHORT: {
                        int16_t value;
                        std::memcpy(&value, ptr + c * sizeof(int16_t), sizeof(value));
                        result[c] = std::max(static_cast<float>(value) / 32767.0f, -1.0f);
                        break;
                    }
                    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        uint16_t value;
                        std::memcpy(&value, ptr + c * sizeof(uint16_t), sizeof(value));
                        result[c] = static_cast<float>(value) / 65535.0f;
                        break;
                    }
                    default:
                        break;
                }
            }
            break;
        default:
            throw std::runtime_error("glTF: unsupported attribute component type");
    }
    return result;
}

// Read one index element as uint32_t.
uint32_t readIndexElement(const tg3_accessor& accessor,
                          const tg3_buffer_view& view,
                          const tg3_buffer& buffer,
                          uint64_t index) {
    const uint8_t* ptr = accessorElementPtr(accessor, view, buffer, index);
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *ptr;
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
            uint16_t value;
            std::memcpy(&value, ptr, sizeof(value));
            return value;
        }
        case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
            uint32_t value;
            std::memcpy(&value, ptr, sizeof(value));
            return value;
        }
        default:
            throw std::runtime_error(
                "glTF: unsupported index component type (expected unsigned byte/short/int)");
    }
}

// Load one primitive, appending its vertices and (triangle-list) indices to
// the output containers.
void loadPrimitive(const tg3_model& model,
                   const tg3_primitive& primitive,
                   std::vector<Vertex>& outVertices,
                   std::vector<uint32_t>& outIndices) {
    // --- POSITION (mandatory) ---
    const int32_t positionIndex = findAttribute(&primitive, "POSITION");
    if (positionIndex < 0) {
        std::cerr << "[glTF] warning: primitive without POSITION skipped" << std::endl;
        return;
    }
    const tg3_accessor& positionAccessor = model.accessors[positionIndex];
    if (positionAccessor.buffer_view < 0 || positionAccessor.sparse.is_sparse ||
        positionAccessor.type != TG3_TYPE_VEC3 || !attributeReadable(positionAccessor) ||
        positionAccessor.count == 0) {
        std::cerr << "[glTF] warning: unsupported POSITION accessor (sparse or non-float), primitive skipped"
                  << std::endl;
        return;
    }
    const uint64_t vertexCount = positionAccessor.count;
    const tg3_buffer_view& positionView = model.buffer_views[positionAccessor.buffer_view];
    const tg3_buffer& positionBuffer = model.buffers[positionView.buffer];

    // --- Optional attributes (NORMAL / TEXCOORD_0) ---
    const tg3_accessor*    normalAccessor = nullptr;
    const tg3_buffer_view* normalView     = nullptr;
    const tg3_buffer*      normalBuffer   = nullptr;
    const tg3_accessor*    uvAccessor     = nullptr;
    const tg3_buffer_view* uvView         = nullptr;
    const tg3_buffer*      uvBuffer       = nullptr;

    const int32_t normalIndex = findAttribute(&primitive, "NORMAL");
    if (normalIndex >= 0) {
        const tg3_accessor& accessor = model.accessors[normalIndex];
        if (accessor.buffer_view < 0 || accessor.sparse.is_sparse ||
            accessor.type != TG3_TYPE_VEC3 || accessor.count < vertexCount ||
            !attributeReadable(accessor)) {
            std::cerr << "[glTF] warning: unsupported NORMAL accessor; smooth normals will be generated"
                      << std::endl;
        } else {
            normalAccessor = &accessor;
            normalView     = &model.buffer_views[accessor.buffer_view];
            normalBuffer   = &model.buffers[normalView->buffer];
        }
    }

    const int32_t uvIndex = findAttribute(&primitive, "TEXCOORD_0");
    if (uvIndex >= 0) {
        const tg3_accessor& accessor = model.accessors[uvIndex];
        if (accessor.buffer_view < 0 || accessor.sparse.is_sparse ||
            accessor.type != TG3_TYPE_VEC2 || accessor.count < vertexCount ||
            !attributeReadable(accessor)) {
            std::cerr << "[glTF] warning: unsupported TEXCOORD_0 accessor; UVs default to (0,0)"
                      << std::endl;
        } else {
            uvAccessor = &accessor;
            uvView     = &model.buffer_views[accessor.buffer_view];
            uvBuffer   = &model.buffers[uvView->buffer];
        }
    }

    // --- Indices ---
    std::vector<uint32_t> localIndices;
    if (primitive.indices >= 0) {
        const tg3_accessor& indexAccessor = model.accessors[primitive.indices];
        if (indexAccessor.buffer_view < 0 || indexAccessor.sparse.is_sparse ||
            indexAccessor.type != TG3_TYPE_SCALAR) {
            std::cerr << "[glTF] warning: unsupported index accessor, primitive skipped" << std::endl;
            return;
        }
        const tg3_buffer_view& indexView = model.buffer_views[indexAccessor.buffer_view];
        const tg3_buffer& indexBuffer = model.buffers[indexView.buffer];
        localIndices.reserve(indexAccessor.count);
        for (uint64_t i = 0; i < indexAccessor.count; ++i) {
            const uint32_t index = readIndexElement(indexAccessor, indexView, indexBuffer, i);
            if (index >= vertexCount) {
                throw std::runtime_error("glTF: primitive index out of range");
            }
            localIndices.emplace_back(index);
        }
    } else {
        // Non-indexed primitive: synthesize sequential indices.
        localIndices.reserve(vertexCount);
        for (uint64_t i = 0; i < vertexCount; ++i) {
            localIndices.emplace_back(static_cast<uint32_t>(i));
        }
    }

    // --- Primitive mode: expand strips/fans to a plain triangle list ---
    const int32_t mode = primitive.mode < 0 ? TG3_MODE_TRIANGLES : primitive.mode;
    std::vector<uint32_t> triangleIndices;
    switch (mode) {
        case TG3_MODE_TRIANGLES:
            triangleIndices.swap(localIndices);
            break;
        case TG3_MODE_TRIANGLE_STRIP:
            triangleIndices.reserve((localIndices.size() - 2) * 3);
            for (uint64_t i = 2; i < localIndices.size(); ++i) {
                const uint32_t a = localIndices[i - 2];
                const uint32_t b = localIndices[i - 1];
                const uint32_t c = localIndices[i];
                triangleIndices.emplace_back(a);
                triangleIndices.emplace_back((i & 1u) ? c : b);
                triangleIndices.emplace_back((i & 1u) ? b : c);
            }
            break;
        case TG3_MODE_TRIANGLE_FAN:
            triangleIndices.reserve((localIndices.size() - 2) * 3);
            for (uint64_t i = 2; i < localIndices.size(); ++i) {
                triangleIndices.emplace_back(localIndices[0]);
                triangleIndices.emplace_back(localIndices[i - 1]);
                triangleIndices.emplace_back(localIndices[i]);
            }
            break;
        default:
            std::cerr << "[glTF] warning: non-triangle primitive mode " << mode << " skipped"
                      << std::endl;
            return;
    }
    if (triangleIndices.empty()) return;

    // --- Fill interleaved vertices ---
    if (outVertices.size() + vertexCount > UINT32_MAX) {
        throw std::runtime_error("glTF: model exceeds uint32 vertex indexing");
    }
    const uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());
    outVertices.reserve(outVertices.size() + vertexCount);
    for (uint64_t i = 0; i < vertexCount; ++i) {
        Vertex vertex{}; 
        vertex.pos = glm::vec3(readAttributeElement(positionAccessor, positionView, positionBuffer, i));
        if (uvAccessor) {
            vertex.texCoord = glm::vec2(readAttributeElement(*uvAccessor, *uvView, *uvBuffer, i));
        } else {
            vertex.texCoord = glm::vec2(0.0f);
        }
        if (normalAccessor) {
            vertex.setNormal(glm::vec3(readAttributeElement(*normalAccessor, *normalView, *normalBuffer, i)));
        } else {
            vertex.setNormal(glm::vec3(0.0f));  // Mesh regenerates smooth normals
        }
        outVertices.emplace_back(vertex);
    }
    for (const uint32_t local : triangleIndices) {
        outIndices.emplace_back(baseVertex + local);
    }
}

}  // namespace

std::unique_ptr<glTFModel> glTFModel::fromglTF(const std::string& modelPath,
                                               VmaAllocator* alloc,
                                               CommandPool& commandPool) {
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;  // geometry-only loader: never decode images

    const tg3_error_code result = tg3_parse_file(model.get(), errors.get(),
                                                 modelPath.c_str(),
                                                 static_cast<uint32_t>(modelPath.size()),
                                                 &options);

    // Dump diagnostics; error strings are arena-owned and valid until model free.
    for (uint32_t i = 0; i < errors.count(); ++i) {
        const tg3_error_entry& entry = *errors.entry(i);
        std::cerr << "[glTF] " << severityName(entry.severity)
                  << " (" << static_cast<int>(entry.code) << ")";
        if (entry.json_path) std::cerr << " at " << entry.json_path;
        std::cerr << ": " << (entry.message ? entry.message : "(no message)") << std::endl;
    }

    if (result != TG3_OK || errors.has_error()) {
        const char* reason = (errors.count() > 0 && errors.entry(0)->message)
                                 ? errors.entry(0)->message
                                 : "unknown parse error";
        throw std::runtime_error("glTF: failed to load " + modelPath + ": " + reason);
    }

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    for (uint32_t m = 0; m < model->meshes_count; ++m) {
        const tg3_mesh& mesh = model->meshes[m];
        for (uint32_t p = 0; p < mesh.primitives_count; ++p) {
            loadPrimitive(*model.get(), mesh.primitives[p], vertices, indices);
        }
    }

    if (vertices.empty() || indices.empty()) {
        throw std::runtime_error("glTF: no triangle geometry found in " + modelPath);
    }

    auto mesh_ = std::make_shared<Mesh>(std::move(vertices), std::move(indices), alloc, commandPool);
    return std::unique_ptr<glTFModel>(new glTFModel(std::move(mesh_)));
}
