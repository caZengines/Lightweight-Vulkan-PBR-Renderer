#pragma once
#include "command_manager.hpp"
#include "descriptor_manager.hpp"
#include "generic/buffer.hpp"
#include "generic/material.hpp"
#include "generic/vertex.hpp"
#include "resource/resource_registry.hpp"
#include <memory>

class RenderObject {
    public:
        // Ban copy
        RenderObject (const RenderObject&) = delete;
        RenderObject& operator=(const RenderObject&) = delete;

        // mesh: asset handle (must be valid; resolved to MeshGPU here and kept
        // alive by the handle for the object's lifetime).
        explicit RenderObject(const resource::AssetHandle& mesh,
                              std::shared_ptr<Material> material,
                              const resource::ResourceRegistry& registry);

        void setInstances(VmaAllocator alloc, const std::vector<InstanceData>& instances,
                          resource::UploadQueue& queue);
        void initMaterialDescriptor(RenderContext& rct,
                                    const vk::DescriptorSetLayout& layout,
                                    const DescriptorPool& pool);
        
        const resource::MeshGPU& getMeshGPU() const { return *meshGPU_; }
        resource::AssetHandle getMeshHandle() const { return meshHandle_; }
        std::shared_ptr<Material> getMaterialShared() const { return material_; }
        const VkBuffer getInstanceBuffer() const { return instanceBuffer_->getBuffer(); }
        uint32_t getInstanceCount() const { return static_cast<uint32_t>(instanceDatas_.size()); }

    private:
        resource::AssetHandle          meshHandle_;
        const resource::MeshGPU*       meshGPU_ = nullptr;
        std::shared_ptr<Material>      material_;
        std::vector<InstanceData>      instanceDatas_;
        std::unique_ptr<Buffer<InstanceData>> instanceBuffer_ = nullptr;
};
