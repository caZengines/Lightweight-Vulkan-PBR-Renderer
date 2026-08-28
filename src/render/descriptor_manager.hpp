#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/frame_resources.hpp"   // kMaxFramesInFlight
#include "render_context.hpp"

namespace Binding {
    constexpr uint32_t kUbo           = 0;
    constexpr uint32_t kAlbedoTexture = 0;
    constexpr uint32_t kNormalTexture = 1;   // (sic) legacy typo kept — shader-facing naming
}  // namespace Binding

struct ReflectBinding {
    uint32_t        binding;
    uint32_t        set;
    vk::DescriptorType   descriptorType;
    uint32_t        count;
    vk::ShaderStageFlags stageFlags;
    std::string          name;       // entry-point name
    uint32_t        blockSize;  // only meaningful for UBO/SSBO

    ReflectBinding& setBinding(uint32_t v) { binding = v; return *this; }
    ReflectBinding& setDescriptorSet(uint32_t v) { set = v; return *this; }
    ReflectBinding& setDescriptorType(vk::DescriptorType v) { descriptorType = v; return *this; }
    ReflectBinding& setDescriptorCount(uint32_t v) { count = v; return *this; }
    ReflectBinding& setShaderStage(vk::ShaderStageFlags v) { stageFlags = v; return *this; }
    ReflectBinding& setEntryPoint(std::string v) { name = std::move(v); return *this; }
    ReflectBinding& setBlockSize(uint32_t v) { blockSize = v; return *this; }
};

namespace render {

// SPIRV-Reflect driven descriptor-set layout derivation: consumes raw SPIR-V
// (from ShaderManager), produces Set layouts plus the matching pool sizes
// (Set 0 scaled by kMaxFramesInFlight, per-object sets by an upper bound).
class DescriptorSetLayout final {
public:
    static constexpr uint32_t kDefaultObjectMultiplier = 8;

    explicit DescriptorSetLayout(RenderContext& rct, const std::vector<uint8_t>& spvCode);
    ~DescriptorSetLayout() = default;

    [[nodiscard]] const std::vector<vk::raii::DescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts_; }
    [[nodiscard]] const std::vector<vk::DescriptorSetLayout>&       getLayoutHandles()       const { return layoutHandles_; }
    [[nodiscard]] const std::vector<ReflectBinding>&                getBindings()            const { return bindings_; }
    [[nodiscard]] int                                               getPoolMaxSets()         const { return poolMaxSets_; }
    [[nodiscard]] int                                               getSetCount()            const { return setCount_; }
    [[nodiscard]] const std::vector<vk::DescriptorPoolSize>&        getPoolSize()            const { return poolSizes_; }

    int computePoolMaxSets(uint32_t objectCount) const;

private:
    void autoCreateDSL(const std::vector<uint8_t>& spvCode);

    RenderContext                                 rct_;
    std::vector<vk::raii::DescriptorSetLayout>    descriptorSetLayouts_;
    std::vector<vk::DescriptorSetLayout>          layoutHandles_;
    std::vector<ReflectBinding>                   bindings_;
    std::vector<vk::DescriptorPoolSize>           poolSizes_;

    int                                           setCount_    = 0;
    int                                           poolMaxSets_ = 0;
};

class DescriptorPool final {
public:
    DescriptorPool(const DescriptorPool&)            = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    explicit DescriptorPool(RenderContext& rct,
                            int maxSets,
                            const std::vector<vk::DescriptorPoolSize>& poolSizes);
    ~DescriptorPool() = default;

    [[nodiscard]] const vk::raii::DescriptorPool& getDescriptorPool() const { return descriptorPool_; }

private:
    RenderContext                    rct_;
    vk::raii::DescriptorPool         descriptorPool_ = nullptr;
};

// NOTE: the unused `DescriptorSet` wrapper and `PerFrameDescriptorSet` were
// deleted in Phase 3 — their jobs now live in FrameResources.

}  // namespace render
