#include "resource/gltf_importer.hpp"

#include "extern/tiny_gltf_v3.h"
#include "platform/log.hpp"

#include <array>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// GltfSceneImporter — full-scene glTF 2.0 CPU import (geometry + hierarchy).
//
// Scope:
//   * .gltf (with external .bin buffers) and .glb files
//   * one GltfPrimitive per triangle primitive — no merging across
//     primitives, because each primitive carries its own glTF material
//   * node hierarchy collapsed into a per-primitive world matrix (matrix and
//     TRS nodes; the glTF default scene is the traversal root)
//   * POSITION (vec3), NORMAL (vec3), TANGENT (vec4 float, w = handedness),
//     TEXCOORD_0 / TEXCOORD_1 (vec2) attributes, float or normalized int
//   * TRIANGLES / TRIANGLE_STRIP / TRIANGLE_FAN (expanded), 8/16/32-bit
//     indices, non-indexed primitives, sparse accessors
//
// Not supported: Draco compression, morph targets, skins/animations (warned
// and skipped). UVs are glTF-native (V top-down, unflipped — see header).
// The vertex pipeline bakes tangents from the file when present; otherwise
// MeshData::postProcess computes them from TEXCOORD_0.
// =============================================================================

namespace {

using log = platform::LogLocator;

const char* severityName(tg3_severity severity) {
    switch (severity) {
        case TG3_SEVERITY_INFO:    return "INFO";
        case TG3_SEVERITY_WARNING: return "WARNING";
        case TG3_SEVERITY_ERROR:   return "ERROR";
        default:                   return "UNKNOWN";
    }
}

std::string toString(tg3_str str) {
    return str.data ? std::string(str.data, str.len) : std::string();
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
// strips/fans on the fly.
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

// Load one primitive into fresh vertex/index containers. Returns false when
// the primitive was skipped (no POSITION / unsupported accessor / non-triangle
// mode); `tangentsFromFile` reports whether trusted TANGENT data was read.
bool loadPrimitive(const tg3_model& model,
                   const tg3_primitive& primitive,
                   std::vector<rhi::Vertex>& outVertices,
                   std::vector<uint32_t>& outIndices,
                   bool& tangentsFromFile) {
    tangentsFromFile = false;

    // --- POSITION (mandatory) ---
    const int32_t positionIndex = findAttribute(&primitive, "POSITION");
    if (positionIndex < 0) {
        log::get().write(platform::LogLevel::Warning,
            "[glTF] warning: primitive without POSITION skipped");
        return false;
    }
    const tg3_accessor& positionAccessor = model.accessors[positionIndex];
    if (positionAccessor.type != TG3_TYPE_VEC3 ||
        !attributeReadable(positionAccessor) || positionAccessor.count == 0) {
        log::get().write(platform::LogLevel::Warning,
            "[glTF] warning: unsupported POSITION accessor, primitive skipped");
        return false;
    }
    const AccessorData positionData = prepareAccessor(model, positionIndex);
    const uint64_t vertexCount = positionData.count();

    // --- Optional attribute loader: validates and prepares, or warns + clears.
    // `requiredComponentCount` guards the vec size; 0 disables the check for
    // attributes that accept any of the glTF-declared sizes.
    auto prepareOptional = [&](const char* name, int32_t requiredType) -> std::optional<AccessorData> {
        const int32_t accessorIndex = findAttribute(&primitive, name);
        if (accessorIndex < 0) {
            return std::nullopt;
        }
        const tg3_accessor& accessor = model.accessors[accessorIndex];
        const bool typeOk = requiredType == 0 || accessor.type == requiredType;
        if (!typeOk || accessor.count < vertexCount || !attributeReadable(accessor)) {
            log::get().write(platform::LogLevel::Warning,
                std::string("[glTF] warning: unsupported ") + name +
                " accessor; attribute defaults are used");
            return std::nullopt;
        }
        return prepareAccessor(model, accessorIndex);
    };

    const std::optional<AccessorData> normalData = prepareOptional("NORMAL", TG3_TYPE_VEC3);
    const std::optional<AccessorData> uv0Data    = prepareOptional("TEXCOORD_0", TG3_TYPE_VEC2);
    const std::optional<AccessorData> uv1Data    = prepareOptional("TEXCOORD_1", TG3_TYPE_VEC2);

    // TANGENT is vec4 (xyz + handedness w) and glTF mandates float here.
    std::optional<AccessorData> tangentData;
    const int32_t tangentIndex = findAttribute(&primitive, "TANGENT");
    if (tangentIndex >= 0) {
        const tg3_accessor& accessor = model.accessors[tangentIndex];
        if (accessor.type == TG3_TYPE_VEC4 &&
            accessor.component_type == TG3_COMPONENT_TYPE_FLOAT &&
            accessor.count >= vertexCount) {
            tangentData.emplace(prepareAccessor(model, tangentIndex));
            tangentsFromFile = true;
        } else {
            log::get().write(platform::LogLevel::Warning,
                "[glTF] warning: unsupported TANGENT accessor (want float vec4); "
                "tangents will be computed from TEXCOORD_0");
        }
    }

    // --- Primitive mode validation ---
    const int32_t mode = primitive.mode < 0 ? TG3_MODE_TRIANGLES : primitive.mode;
    if (mode != TG3_MODE_TRIANGLES && mode != TG3_MODE_TRIANGLE_STRIP &&
        mode != TG3_MODE_TRIANGLE_FAN) {
        log::get().write(platform::LogLevel::Warning,
            "[glTF] warning: non-triangle primitive mode " + std::to_string(mode) + " skipped");
        return false;
    }

    // --- Indices ---
    std::optional<AccessorData> indexData;
    uint64_t indexCount = vertexCount;  // Non-indexed primitive.
    if (primitive.indices >= 0) {
        const tg3_accessor& indexAccessor = model.accessors[primitive.indices];
        if (indexAccessor.type != TG3_TYPE_SCALAR ||
            !indexComponentReadable(indexAccessor)) {
            log::get().write(platform::LogLevel::Warning,
                "[glTF] warning: unsupported index accessor, primitive skipped");
            return false;
        }
        indexData.emplace(prepareAccessor(model, primitive.indices));
        indexCount = indexData->count();
    }
    if (indexCount == 0) {
        return false;
    }
    if (mode != TG3_MODE_TRIANGLES && indexCount < 3) {
        return false;  // Not enough indices for a strip/fan triangle.
    }

    // --- Fill interleaved vertices ---
    if (outVertices.size() + vertexCount > UINT32_MAX) {
        throw std::runtime_error("glTF: model exceeds uint32 vertex indexing");
    }
    const uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());
    reserveAdditional(outVertices, vertexCount);
    for (uint64_t i = 0; i < vertexCount; ++i) {
        rhi::Vertex vertex{};
        vertex.pos = glm::vec3(readAttributeElement(positionData, i));
        vertex.texCoord  = uv0Data ? glm::vec2(readAttributeElement(*uv0Data, i))
                                   : glm::vec2(0.0f);
        vertex.texCoord1 = uv1Data ? glm::vec2(readAttributeElement(*uv1Data, i))
                                   : glm::vec2(0.0f);
        if (normalData) {
            vertex.setNormal(glm::vec3(readAttributeElement(*normalData, i)));
        } else {
            vertex.setNormal(glm::vec3(0.0f));  // postProcess regenerates smooth normals
        }
        if (tangentData) {
            const glm::vec4 t = readAttributeElement(*tangentData, i);
            vertex.setTangent(glm::vec3(t), t.w);
        } else {
            vertex.setTangent(glm::vec3(0.0f), 0.0f);  // zero = "compute me" marker
        }
        outVertices.emplace_back(vertex);
    }

    appendPrimitiveIndices(indexData ? &*indexData : nullptr, indexCount,
                           vertexCount, mode, baseVertex, outIndices);
    return true;
}

// Local matrix of one node: explicit matrix (glTF stores it column-major,
// same as glm) or composed TRS (glTF rotation quaternion is [x, y, z, w],
// glm's scalar quaternion constructor takes (w, x, y, z)).
glm::mat4 nodeLocalMatrix(const tg3_node& node) {
    if (node.has_matrix) {
        std::array<float, 16> m{};
        for (int32_t i = 0; i < 16; ++i) {
            m[i] = static_cast<float>(node.matrix[i]);
        }
        return glm::make_mat4(m.data());
    }
    const glm::quat rotation(node.rotation[3], node.rotation[0],
                             node.rotation[1], node.rotation[2]);
    return glm::translate(glm::mat4(1.0f),
                          glm::vec3(node.translation[0], node.translation[1], node.translation[2])) *
           glm::mat4_cast(rotation) *  
           glm::scale(glm::mat4(1.0f),
                      glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
}

// Depth-first walk of the node graph. `onStack` is a path marker, not a
// global visited set: a node referenced by two parents (legal DAG) is emitted
// once per reference, while a cycle is a hard error. Each mesh node emits one
// GltfPrimitive per drawable primitive.
void visitNode(const tg3_model& model,
               int32_t nodeIndex,
               const glm::mat4& parentWorld,
               std::vector<bool>& onStack,
               resource::GltfScene& out) {
    if (nodeIndex < 0 || nodeIndex >= model.nodes_count) {
        throw std::runtime_error("glTF: scene references an invalid node");
    }
    if (onStack[static_cast<size_t>(nodeIndex)]) {
        throw std::runtime_error("glTF: node graph contains a cycle");
    }
    onStack[static_cast<size_t>(nodeIndex)] = true;

    const tg3_node& node = model.nodes[nodeIndex];
    const glm::mat4 world = parentWorld * nodeLocalMatrix(node);

    if (node.mesh >= 0) {
        if (static_cast<uint32_t>(node.mesh) >= model.meshes_count) {
            throw std::runtime_error("glTF: node references an invalid mesh");
        }
        const tg3_mesh& mesh = model.meshes[node.mesh];
        const std::string meshName = toString(mesh.name);
        for (uint32_t p = 0; p < mesh.primitives_count; ++p) {
            std::vector<rhi::Vertex> vertices;
            std::vector<uint32_t>    indices;
            bool tangentsFromFile = false;
            if (!loadPrimitive(model, mesh.primitives[p], vertices, indices,
                               tangentsFromFile)) {
                continue;
            }
            resource::GltfPrimitive primitiveOut;
            primitiveOut.mesh = resource::MeshData(std::move(vertices), std::move(indices),
                                                   tangentsFromFile);
            primitiveOut.mesh.postProcess();
            primitiveOut.world = world;
            primitiveOut.materialIndex =
                mesh.primitives[p].material >= 0
                    ? static_cast<uint32_t>(mesh.primitives[p].material)
                    : 0xFFFFFFFFu;
            primitiveOut.name = meshName.empty()
                                    ? "mesh_" + std::to_string(node.mesh)
                                    : meshName;
            out.primitives.emplace_back(std::move(primitiveOut));
        }
    }

    for (uint32_t c = 0; c < node.children_count; ++c) {
        visitNode(model, node.children[c], world, onStack, out);
    }

    onStack[static_cast<size_t>(nodeIndex)] = false;
}

}  // namespace

namespace resource {

GltfScene GltfSceneImporter::load(const std::string& modelPath) {
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;

    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;       // never decode images here — textures are
                                    // decoded on demand by the app layer (P2)
    options.skip_extras_values = 1; // don't materialize extras/extension values
    options.parse_float32 = 1;      // glTF geometry values are single-precision

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
        log::get().write(platform::LogLevel::Warning, diagnostic);
    }

    if (result != TG3_OK || errors.has_error()) {
        const char* reason = (errors.count() > 0 && errors.entry(0)->message)
                                 ? errors.entry(0)->message
                                 : "unknown parse error";
        throw std::runtime_error("glTF: failed to load " + modelPath + ": " + reason);
    }

    GltfScene scene;
    if (model->scenes_count == 0) {
        throw std::runtime_error("glTF: no scene in " + modelPath);
    }
    const int32_t defaultScene =
        model->default_scene >= 0 ? model->default_scene : 0;
    if (static_cast<uint32_t>(defaultScene) >= model->scenes_count) {
        throw std::runtime_error("glTF: default scene index out of range in " + modelPath);
    }

    std::vector<bool> onStack(model->nodes_count, false);
    const tg3_scene& sceneRoot = model->scenes[defaultScene];
    for (uint32_t n = 0; n < sceneRoot.nodes_count; ++n) {
        visitNode(*model.get(), sceneRoot.nodes[n], glm::mat4(1.0f), onStack, scene);
    }

    if (scene.primitives.empty()) {
        throw std::runtime_error("glTF: no drawable triangle geometry in " + modelPath);
    }
    return scene;
}

}  // namespace resource
