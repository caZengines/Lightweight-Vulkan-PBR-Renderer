#pragma once
#include "render_context.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace Binding {
    constexpr uint32_t kUbo             = 0;
    constexpr uint32_t kAlbedoTexture   = 1;
    constexpr uint32_t kNormalTexure    = 2;
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

        vk::raii::DescriptorSetLayout& getDescriptorSetLayout() { return descriptorSetLayout; }

        const ReflectBinding* getBinding(const std::string& name) const;
        const std::vector<ReflectBinding>& getBindings() const { return bindings_; }
        const std::vector<vk::DescriptorPoolSize>& getPoolSize() const { return poolSizes; }

    private:
        RenderContext                            rct_;
        vk::raii::DescriptorSetLayout            descriptorSetLayout  = nullptr;
        std::vector<ReflectBinding>              bindings_;
        std::vector<vk::DescriptorPoolSize>      poolSizes;

        void createDescriptorSetLayout();
        void autoCreateDSL(const std::vector<uint8_t>& spvCode_);
};

class DescriptorPool {
    public:
        //Ban copying
        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;

        explicit DescriptorPool(RenderContext& rct, 
                                const std::vector<vk::DescriptorPoolSize>& poolSizes
                            );
        ~DescriptorPool() = default;

        const vk::raii::DescriptorPool& getDescriptorPool() const { return descriptorPool; }

    private:
        RenderContext                            rct_;
        vk::raii::DescriptorPool                 descriptorPool      = nullptr;
};