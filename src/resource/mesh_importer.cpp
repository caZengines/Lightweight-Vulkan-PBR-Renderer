#include "resource/mesh_importer.hpp"
#include "resource/mesh_data.hpp"

#include "extern/tiny_gltf_v3.h"
#include "platform/log.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include "extern/tiny_obj_loader.h"

// =============================================================================
// MeshImporter — CPU-side geometry importers (OBJ + glTF 2.0).
//
// Both importers produce a post-processed resource::MeshData (deduplication,
// smooth normal generation, tangent computation in MeshData::postProcess).
// GPU upload is the job of ResourceRegistry/UploadQueue.
// =============================================================================

// =============================================================================
// glTF 2.0 loading through the tinygltf v3 C API.
//
// Scope (basic geometry):
//   * .gltf (with external .bin buffers) and .glb files
//   * POSITION (vec3 float), NORMAL (vec3 float or normalized int) and
//     TEXCOORD_0 (vec2 float or normalized int) attributes
//   * TRIANGLES / TRIANGLE_STRIP / TRIANGLE_FAN primitives (strips and fans
//     are expanded to a triangle list); point/line modes are skipped
//   * unsigned byte/short/int index accessors, or none (non-indexed)
//   * sparse accessors, including sparse-only accessors whose base data is
//     implicitly zero-filled
//
// Not supported yet: Draco compression, morph targets, materials/textures,
// node transforms (warned and skipped/filled with defaults). Missing normals
// are regenerated as smooth normals and tangents are computed by postProcess.
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

bool checkedAdd(uint64_t a, uint64_t b, uint64_t& out) {
    if (a > std::numeric_limits<uint64_t>::max() - b) {
        return false;
    }
    out = a + b;
    return true;
}

bool checkedMul(uint64_t a, uint64_t b, uint64_t& out) {
    if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
        return false;
    }
    out = a * b;
    return true;
}

uint64_t accessorElementSize(const tg3_accessor& accessor) {
    const int32_t componentSize  = tg3_component_size(accessor.component_type);
    const int32_t componentCount = tg3_num_components(accessor.type);
    if (componentSize <= 0 || componentCount <= 0) {
        throw std::runtime_error("glTF: accessor with unsupported component type or type");
    }
    return static_cast<uint64_t>(componentSize) * static_cast<uint64_t>(componentCount);
}

// Describes how to read one accessor. The base data (when present) is
// validated once, so per-element reads are just pointer arithmetic. Sparse
// substitutions are stored separately and resolved during sequential reads.
struct AccessorData {
    const tg3_accessor*    accessor = nullptr;
    const tg3_buffer_view* baseView = nullptr;
    const tg3_buffer*      baseBuffer = nullptr;
    int32_t                componentSize = 0;
    int32_t                componentCount = 0;
    uint64_t               elementSize = 0;
    uint64_t               baseStride = 0;

    struct SparseSubstitution {
        uint64_t       targetIndex;
        const uint8_t* valueData;
    };
    std::vector<SparseSubstitution> sparseSubstitutions;
    mutable size_t                  sparseCursor = 0;

    uint64_t count() const { return accessor ? accessor->count : 0; }
};

// Verify that [offset_in_view, offset_in_view + byte_length) lies inside both
// the buffer view and the underlying buffer.
void checkBufferViewRange(const tg3_buffer_view& view,
                          const tg3_buffer& buffer,
                          uint64_t offsetInView,
                          uint64_t byteLength,
                          const char* what) {
    if (offsetInView > view.byte_length || byteLength > view.byte_length - offsetInView) {
        throw std::runtime_error(std::string("glTF: ") + what +
                                 " exceeds its buffer view");
    }
    uint64_t bufferBegin = 0;
    uint64_t bufferEnd = 0;
    if (!checkedAdd(view.byte_offset, offsetInView, bufferBegin) ||
        !checkedAdd(bufferBegin, byteLength, bufferEnd)) {
        throw std::runtime_error(std::string("glTF: ") + what + " buffer range overflows");
    }
    if (bufferEnd > buffer.data.count ||
        (byteLength > 0 && buffer.data.data == nullptr)) {
        throw std::runtime_error(std::string("glTF: ") + what +
                                 " exceeds its buffer");
    }
}

const tg3_buffer_view& getBufferView(const tg3_model& model, int32_t index) {
    if (index < 0 || static_cast<uint32_t>(index) >= model.buffer_views_count) {
        throw std::runtime_error("glTF: accessor buffer view index out of range");
    }
    return model.buffer_views[index];
}

const tg3_buffer& getBuffer(const tg3_model& model, const tg3_buffer_view& view) {
    if (view.buffer < 0 || static_cast<uint32_t>(view.buffer) >= model.buffers_count) {
        throw std::runtime_error("glTF: buffer view references an invalid buffer");
    }
    return model.buffers[view.buffer];
}

// Collect sparse substitutions and validate the sparse index/value streams.
// glTF requires sparse indices to be strictly increasing, so we both validate
// that property and exploit it later with a single monotonic cursor.
void prepareSparseAccessor(const tg3_model& model, AccessorData& data) {
    const tg3_accessor& accessor = *data.accessor;
    const tg3_accessor_sparse& sparse = accessor.sparse;
    if (!sparse.is_sparse || sparse.count == 0) {
        return;
    }
    if (sparse.count < 0 || static_cast<uint64_t>(sparse.count) > accessor.count) {
        throw std::runtime_error("glTF: sparse accessor count is invalid");
    }

    const uint64_t sparseCount = static_cast<uint64_t>(sparse.count);
    if (sparseCount > static_cast<uint64_t>(data.sparseSubstitutions.max_size())) {
        throw std::runtime_error("glTF: sparse accessor is too large");
    }

    // --- Sparse indices ---
    const tg3_buffer_view& indexView = getBufferView(model, sparse.indices.buffer_view);
    const tg3_buffer& indexBuffer = getBuffer(model, indexView);
    if (indexView.byte_stride != 0) {
        throw std::runtime_error("glTF: sparse index buffer view must be tightly packed");
    }
    const int32_t indexSize = tg3_component_size(sparse.indices.component_type);
    if (indexSize <= 0 ||
        (sparse.indices.component_type != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
         sparse.indices.component_type != TG3_COMPONENT_TYPE_UNSIGNED_SHORT &&
         sparse.indices.component_type != TG3_COMPONENT_TYPE_UNSIGNED_INT)) {
        throw std::runtime_error("glTF: unsupported sparse index component type");
    }
    uint64_t indexBytes = 0;
    if (!checkedMul(sparseCount, static_cast<uint64_t>(indexSize), indexBytes)) {
        throw std::runtime_error("glTF: sparse index byte range overflows");
    }
    checkBufferViewRange(indexView, indexBuffer, sparse.indices.byte_offset,
                         indexBytes, "sparse accessor indices");

    // --- Sparse values (tightly packed, same component layout as the accessor) ---
    const tg3_buffer_view& valueView = getBufferView(model, sparse.values.buffer_view);
    const tg3_buffer& valueBuffer = getBuffer(model, valueView);
    if (valueView.byte_stride != 0) {
        throw std::runtime_error("glTF: sparse value buffer view must be tightly packed");
    }
    uint64_t valueBytes = 0;
    if (!checkedMul(sparseCount, data.elementSize, valueBytes)) {
        throw std::runtime_error("glTF: sparse value byte range overflows");
    }
    checkBufferViewRange(valueView, valueBuffer, sparse.values.byte_offset,
                         valueBytes, "sparse accessor values");

    const uint8_t* indexData =
        indexBuffer.data.data + indexView.byte_offset + sparse.indices.byte_offset;
    const uint8_t* valueData =
        valueBuffer.data.data + valueView.byte_offset + sparse.values.byte_offset;

    data.sparseSubstitutions.reserve(static_cast<size_t>(sparseCount));
    uint64_t previousIndex = 0;
    bool hasPrevious = false;
    for (uint64_t i = 0; i < sparseCount; ++i) {
        uint64_t targetIndex = 0;
        const uint8_t* indexPtr = indexData + i * static_cast<uint64_t>(indexSize);
        switch (sparse.indices.component_type) {
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                targetIndex = *indexPtr;
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
                uint16_t value;
                std::memcpy(&value, indexPtr, sizeof(value));
                targetIndex = value;
                break;
            }
            case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
                uint32_t value;
                std::memcpy(&value, indexPtr, sizeof(value));
                targetIndex = value;
                break;
            }
            default:
                break;
        }
        if (targetIndex >= accessor.count) {
            throw std::runtime_error("glTF: sparse accessor index is out of range");
        }
        if (hasPrevious && targetIndex <= previousIndex) {
            throw std::runtime_error("glTF: sparse accessor indices must be strictly increasing");
        }
        previousIndex = targetIndex;
        hasPrevious = true;
        data.sparseSubstitutions.push_back(
            {targetIndex, valueData + i * data.elementSize});
    }
}

AccessorData prepareAccessor(const tg3_model& model, int32_t accessorIndex) {
    if (accessorIndex < 0 || static_cast<uint32_t>(accessorIndex) >= model.accessors_count) {
        throw std::runtime_error("glTF: primitive references an invalid accessor");
    }

    AccessorData data;
    data.accessor = &model.accessors[accessorIndex];
    data.componentSize = tg3_component_size(data.accessor->component_type);
    data.componentCount = tg3_num_components(data.accessor->type);
    data.elementSize = accessorElementSize(*data.accessor);

    if (data.accessor->buffer_view >= 0) {
        data.baseView = &getBufferView(model, data.accessor->buffer_view);
        data.baseBuffer = &getBuffer(model, *data.baseView);
        const int32_t stride =
            tg3_accessor_byte_stride(data.accessor, data.baseView);
        if (stride <= 0 || static_cast<uint64_t>(stride) < data.elementSize) {
            throw std::runtime_error("glTF: accessor has an invalid byte stride");
        }
        data.baseStride = static_cast<uint64_t>(stride);

        // Validate the complete base range once instead of checking every
        // element while copying vertices.
        uint64_t baseSpan = 0;
        if (data.accessor->count > 0) {
            uint64_t lastElementOffset = 0;
            if (!checkedMul(data.accessor->count - 1u, data.baseStride,
                            lastElementOffset) ||
                !checkedAdd(lastElementOffset, data.elementSize, baseSpan)) {
                throw std::runtime_error("glTF: accessor byte range overflows");
            }
        }
        checkBufferViewRange(*data.baseView, *data.baseBuffer,
                             data.accessor->byte_offset, baseSpan,
                             "accessor");
    } else {
        // A missing base buffer view is valid for sparse accessors; base
        // elements are zero. readAttributeElement/readIndexElement treat a
        // null element pointer as a zero value.
        data.baseStride = data.elementSize;
    }

    prepareSparseAccessor(model, data);
    return data;
}

// Return the source bytes for one accessor element. A null pointer means the
// element belongs to a zero-filled (buffer-view-less) base and no sparse
// substitution applies.
const uint8_t* accessorElementPtr(const AccessorData& data, uint64_t index) {
    if (index >= data.count()) {
        throw std::runtime_error("glTF: accessor element index out of range");
    }

    if (!data.sparseSubstitutions.empty()) {
        while (data.sparseCursor < data.sparseSubstitutions.size() &&
               data.sparseSubstitutions[data.sparseCursor].targetIndex < index) {
            ++data.sparseCursor;
        }
        if (data.sparseCursor < data.sparseSubstitutions.size() &&
            data.sparseSubstitutions[data.sparseCursor].targetIndex == index) {
            return data.sparseSubstitutions[data.sparseCursor].valueData;
        }
    }

    if (!data.baseView || !data.baseBuffer) {
        return nullptr;
    }
    return data.baseBuffer->data.data + data.baseView->byte_offset +
           data.accessor->byte_offset + index * data.baseStride;
}

// Read one attribute element as a float4. The accessor must have been
// accepted by attributeReadable().
glm::vec4 readAttributeElement(const AccessorData& data, uint64_t index) {
    const tg3_accessor& accessor = *data.accessor;
    const uint8_t* ptr = accessorElementPtr(data, index);
    if (!ptr) {
        return glm::vec4(0.0f);
    }

    glm::vec4 result(0.0f);
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_FLOAT:
            for (int32_t c = 0; c < data.componentCount; ++c) {
                float value;
                std::memcpy(&value, ptr + c * sizeof(float), sizeof(value));
                result[c] = value;
            }
            break;
        case TG3_COMPONENT_TYPE_DOUBLE:
            for (int32_t c = 0; c < data.componentCount; ++c) {
                double value;
                std::memcpy(&value, ptr + c * sizeof(double), sizeof(value));
                result[c] = static_cast<float>(value);
            }
            break;
        case TG3_COMPONENT_TYPE_BYTE:
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TG3_COMPONENT_TYPE_SHORT:
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (int32_t c = 0; c < data.componentCount; ++c) {
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
uint32_t readIndexElement(const AccessorData& data, uint64_t index) {
    const tg3_accessor& accessor = *data.accessor;
    const uint8_t* ptr = accessorElementPtr(data, index);
    if (!ptr) {
        return 0;  // Zero-filled accessor without a base buffer view.
    }
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

template <typename T>
void reserveAdditional(std::vector<T>& target, uint64_t additional) {
    const uint64_t maxSize = static_cast<uint64_t>(target.max_size());
    const uint64_t currentSize = static_cast<uint64_t>(target.size());
    if (additional > maxSize || currentSize > maxSize - additional) {
        throw std::runtime_error("glTF: geometry is too large");
    }
    target.reserve(static_cast<size_t>(currentSize + additional));
}

bool indexComponentReadable(const tg3_accessor& accessor) {
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:
            return true;
        default:
            return false;
    }
}

// Append triangle-list indices directly to outIndices. This expands triangle
// strips/fans on the fly and avoids the temporary localIndices/triangleIndices
// vectors used by the previous implementation.
void appendPrimitiveIndices(const AccessorData* indexData,
                            uint64_t indexCount,
                            uint64_t vertexCount,
                            int32_t mode,
                            uint32_t baseVertex,
                            std::vector<uint32_t>& outIndices) {
    if (mode != TG3_MODE_TRIANGLES && indexCount < 3) {
        return;
    }
    auto readLocal = [&](uint64_t i) -> uint32_t {
        if (indexData) {
            const uint32_t index = readIndexElement(*indexData, i);
            if (index >= vertexCount) {
                throw std::runtime_error("glTF: primitive index out of range");
            }
            return index;
        }
        if (i >= vertexCount || i > UINT32_MAX) {
            throw std::runtime_error("glTF: primitive index out of range");
        }
        return static_cast<uint32_t>(i);
    };

    switch (mode) {
        case TG3_MODE_TRIANGLES:
            reserveAdditional(outIndices, indexCount);
            for (uint64_t i = 0; i < indexCount; ++i) {
                outIndices.emplace_back(baseVertex + readLocal(i));
            }
            break;

        case TG3_MODE_TRIANGLE_STRIP: {
            uint64_t triangleCount = 0;
            if (!checkedMul(indexCount - 2u, 3u, triangleCount)) {
                throw std::runtime_error("glTF: expanded triangle count overflows");
            }
            reserveAdditional(outIndices, triangleCount);
            uint32_t a = readLocal(0);
            uint32_t b = readLocal(1);
            for (uint64_t i = 2; i < indexCount; ++i) {
                const uint32_t c = readLocal(i);
                outIndices.emplace_back(baseVertex + a);
                outIndices.emplace_back(baseVertex + ((i & 1u) ? c : b));
                outIndices.emplace_back(baseVertex + ((i & 1u) ? b : c));
                a = b;
                b = c;
            }
            break;
        }

        case TG3_MODE_TRIANGLE_FAN: {
            uint64_t triangleCount = 0;
            if (!checkedMul(indexCount - 2u, 3u, triangleCount)) {
                throw std::runtime_error("glTF: expanded triangle count overflows");
            }
            reserveAdditional(outIndices, triangleCount);
            const uint32_t first = readLocal(0);
            uint32_t previous = readLocal(1);
            for (uint64_t i = 2; i < indexCount; ++i) {
                const uint32_t current = readLocal(i);
                outIndices.emplace_back(baseVertex + first);
                outIndices.emplace_back(baseVertex + previous);
                outIndices.emplace_back(baseVertex + current);
                previous = current;
            }
            break;
        }

        default:
            throw std::runtime_error("glTF: unsupported primitive mode");
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
        platform::LogLocator::get().write(platform::LogLevel::Warning,
            "[glTF] warning: primitive without POSITION skipped");
        return;
    }
    const tg3_accessor& positionAccessor = model.accessors[positionIndex];
    if (positionAccessor.type != TG3_TYPE_VEC3 ||
        !attributeReadable(positionAccessor) || positionAccessor.count == 0) {
        platform::LogLocator::get().write(platform::LogLevel::Warning,
            "[glTF] warning: unsupported POSITION accessor, primitive skipped");
        return;
    }
    const AccessorData positionData = prepareAccessor(model, positionIndex);
    const uint64_t vertexCount = positionData.count();

    // --- Optional attributes (NORMAL / TEXCOORD_0) ---
    std::optional<AccessorData> normalData;
    const int32_t normalIndex = findAttribute(&primitive, "NORMAL");
    if (normalIndex >= 0) {
        const tg3_accessor& accessor = model.accessors[normalIndex];
        if (accessor.type != TG3_TYPE_VEC3 || accessor.count < vertexCount ||
            !attributeReadable(accessor)) {
            platform::LogLocator::get().write(platform::LogLevel::Warning,
                "[glTF] warning: unsupported NORMAL accessor; smooth normals will be generated");
        } else {
            normalData.emplace(prepareAccessor(model, normalIndex));
        }
    }

    std::optional<AccessorData> uvData;
    const int32_t uvIndex = findAttribute(&primitive, "TEXCOORD_0");
    if (uvIndex >= 0) {
        const tg3_accessor& accessor = model.accessors[uvIndex];
        if (accessor.type != TG3_TYPE_VEC2 || accessor.count < vertexCount ||
            !attributeReadable(accessor)) {
            platform::LogLocator::get().write(platform::LogLevel::Warning,
                "[glTF] warning: unsupported TEXCOORD_0 accessor; UVs default to (0,0)");
        } else {
            uvData.emplace(prepareAccessor(model, uvIndex));
        }
    }

    // --- Primitive mode validation ---
    const int32_t mode = primitive.mode < 0 ? TG3_MODE_TRIANGLES : primitive.mode;
    if (mode != TG3_MODE_TRIANGLES && mode != TG3_MODE_TRIANGLE_STRIP &&
        mode != TG3_MODE_TRIANGLE_FAN) {
        platform::LogLocator::get().write(platform::LogLevel::Warning,
            "[glTF] warning: non-triangle primitive mode " + std::to_string(mode) + " skipped");
        return;
    }

    // --- Indices ---
    std::optional<AccessorData> indexData;
    uint64_t indexCount = vertexCount;  // Non-indexed primitive.
    if (primitive.indices >= 0) {
        const tg3_accessor& indexAccessor = model.accessors[primitive.indices];
        if (indexAccessor.type != TG3_TYPE_SCALAR ||
            !indexComponentReadable(indexAccessor)) {
            platform::LogLocator::get().write(platform::LogLevel::Warning,
                "[glTF] warning: unsupported index accessor, primitive skipped");
            return;
        }
        indexData.emplace(prepareAccessor(model, primitive.indices));
        indexCount = indexData->count();
    }
    if (indexCount == 0) {
        return;
    }
    if (mode != TG3_MODE_TRIANGLES && indexCount < 3) {
        return;  // Not enough indices for a strip/fan triangle.
    }

    // --- Fill interleaved vertices ---
    if (outVertices.size() + vertexCount > UINT32_MAX) {
        throw std::runtime_error("glTF: model exceeds uint32 vertex indexing");
    }
    const uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());
    reserveAdditional(outVertices, vertexCount);
    for (uint64_t i = 0; i < vertexCount; ++i) {
        Vertex vertex{};
        vertex.pos = glm::vec3(readAttributeElement(positionData, i));
        if (uvData) {
            vertex.texCoord = glm::vec2(readAttributeElement(*uvData, i));
        } else {
            vertex.texCoord = glm::vec2(0.0f);
        }
        if (normalData) {
            vertex.setNormal(glm::vec3(readAttributeElement(*normalData, i)));
        } else {
            vertex.setNormal(glm::vec3(0.0f));  // postProcess regenerates smooth normals
        }
        outVertices.emplace_back(vertex);
    }

    appendPrimitiveIndices(indexData ? &*indexData : nullptr, indexCount,
                           vertexCount, mode, baseVertex, outIndices);
}

}  // namespace

namespace resource {

MeshData MeshImporter::loadObj(const std::string& modelPath) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str()))
    {
        throw std::runtime_error(err.empty() ? "Failed to load OBJ: " + modelPath : err);
    }
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    size_t cornerCount = 0;
    for (const auto& shape : shapes) cornerCount += shape.mesh.indices.size();
    vertices.reserve(cornerCount);
    indices.reserve(cornerCount);
    const bool hasFileNormals = !attrib.normals.empty();
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.texCoord = {0.0f, 0.0f};
            }
            if (hasFileNormals && index.normal_index >= 0) {
                vertex.setNormal(glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                ));
            } else {
                // Missing normals are generated as smooth normals in postProcess
                vertex.setNormal({0.0f, 0.0f, 0.0f});
            }
            vertices.emplace_back(vertex);
            indices.emplace_back(static_cast<uint32_t>(indices.size()));
        }
    }
    MeshData data(std::move(vertices), std::move(indices));
    data.postProcess();
    return data;
}

MeshData MeshImporter::loadGlTF(const std::string& modelPath) {
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;       // geometry-only loader: never decode images
    options.skip_extras_values = 1; // geometry-only loader: don't materialize extras/extension values
    options.parse_float32 = 1;      // glTF geometry values are single-precision; faster JSON parsing

    const tg3_error_code result = tg3_parse_file(model.get(), errors.get(),
                                                 modelPath.c_str(),
                                                 static_cast<uint32_t>(modelPath.size()),
                                                 &options);

    // Dump diagnostics; error strings are arena-owned and valid until model free.
    for (uint32_t i = 0; i < errors.count(); ++i) {
        const tg3_error_entry& entry = *errors.entry(i);
        std::string diagnostic = "[glTF] " + std::string(severityName(entry.severity)) +
                                 " (" + std::to_string(static_cast<int>(entry.code)) + ")";
        if (entry.json_path) diagnostic += std::string(" at ") + entry.json_path;
        diagnostic += std::string(": ") + (entry.message ? entry.message : "(no message)");
        platform::LogLocator::get().write(platform::LogLevel::Warning, diagnostic);
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

    MeshData data(std::move(vertices), std::move(indices));
    data.postProcess();
    return data;
}

MeshData MeshImporter::load(const std::string& path) {
    std::filesystem::path p(path);
    if (p.extension() == ".gltf" || p.extension() == ".glb") {
        return loadGlTF(path);
    }
    return loadObj(path);
}

}  // namespace resource
