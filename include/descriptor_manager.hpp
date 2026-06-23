#pragma once
#include "render_context.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace Binding {
    constexpr uint32_t kUbo             = 0;
    constexpr uint32_t kAlbedoTexture   = 0;
    constexpr uint32_t kNormalTexure    = 1;
 }

struct ReflectBinding {
    uint32_t                binding;
    uint32_t                set;
    vk::DescriptorType      descriptorType;
    uint32_t                count;
    vk::ShaderStageFlags    stageFlags;
    std::string             name; //EntryPoint Name 
    uint32_t                blockSize; //Only valid for UBO/SSBO

    ReflectBinding& setBinding(uint32_t binding_) { binding = binding_; return *this; }
    ReflectBinding& setDescriptorSet(uint32_t set_) { set = set_; return *this; }
    ReflectBinding& setDescriptorType(vk::DescriptorType descriptorType_) { descriptorType = descriptorType_; return *this; }
    ReflectBinding& setDescriptorCount(uint32_t count_) { count = count_; return *this; }
    ReflectBinding& setShaderStage(vk::ShaderStageFlags stageFlags_) { stageFlags = stageFlags_; return *this; }
    ReflectBinding& setEntryPoint(std::string name_) { name = name_; return *this; }
    ReflectBinding& setBlockSize(uint32_t bs_) { blockSize = bs_; return *this; }
};

//Auto create DescriptorSetLayout by SPRIV-Reflect
class DescriptorSetLayout {
    public:
        explicit DescriptorSetLayout(RenderContext& rct, const std::vector<uint8_t>& spvCode);
        ~DescriptorSetLayout() = default;

        //The binding information saved by describorSetLayouts is in units of each Set in the shader
        const std::vector<vk::raii::DescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts; }
        const std::vector<vk::DescriptorSetLayout>& getLayoutHandles() const { return layouthandles; }

        const ReflectBinding* getBinding(const std::string& name) const;
        const std::vector<ReflectBinding>& getBindings() const { return bindings_; }
        const int& getPoolMaxSets() const { return poolMaxSets; }
        const int& getSetCount() const { return setCount; }
        const std::vector<vk::DescriptorPoolSize>& getPoolSize() const { return poolSizes; }

    private:
        RenderContext                                     rct_;

        std::vector<vk::raii::DescriptorSetLayout>        descriptorSetLayouts;
        std::vector<vk::DescriptorSetLayout>              layouthandles;
        std::vector<ReflectBinding>                       bindings_;
        std::vector<vk::DescriptorPoolSize>               poolSizes;

        int                                               setCount = 0;
        int                                               poolMaxSets = 0;

        void createDescriptorSetLayout();
        void autoCreateDSL(const std::vector<uint8_t>& spvCode_);
};

class DescriptorPool {
    public:
        //Ban copying
        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;

        explicit DescriptorPool(RenderContext& rct,
                                int maxSets,
                                const std::vector<vk::DescriptorPoolSize>& poolSizes
                            );
        ~DescriptorPool() = default;

        const vk::raii::DescriptorPool& getDescriptorPool() const { return descriptorPool; }

    private:
        RenderContext                            rct_;
        vk::raii::DescriptorPool                 descriptorPool      = nullptr;
};

class DescriptorSet{
    public:
        //Ban copying
        DescriptorSet(const DescriptorSet&) = delete;
        DescriptorSet& operator=(const DescriptorSet&) = delete;

        explicit DescriptorSet(RenderContext& rct, vk::DescriptorSetAllocateInfo allocInfo_);
         ~DescriptorSet() = default;

        const std::vector<vk::raii::DescriptorSet>& getDescriptorSets() const { return descriptorSets; }
        const std::vector<vk::DescriptorSet>& getSetsHandles() const { return handles; }
    private:
        RenderContext                                   rct_;
        std::vector<vk::raii::DescriptorSet>            descriptorSets;
        std::vector<vk::DescriptorSet>                  handles;
};