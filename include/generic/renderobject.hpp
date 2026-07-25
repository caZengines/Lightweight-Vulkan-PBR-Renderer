#pragma once
#include "command_manager.hpp"
#include "descriptor_manager.hpp"
#include "generic/material.hpp"
#include "generic/mesh.hpp"
#include "generic/buffer.hpp"
#include "generic/vertex.hpp"
#include <memory>

class RenderObject {
    public:
        //Ban copy
        RenderObject (const RenderObject&) = delete;
        RenderObject& operator=(const RenderObject&) = delete;

        explicit RenderObject(std::shared_ptr<const Mesh> mesh,
                              std::shared_ptr<Material> material);

        void setInstances(const std::vector<InstanceData>& instances, CommandPool& cmdPool);
        void initMaterialDescriptor(RenderContext& rct,
                                    const vk::DescriptorSetLayout& layout,
                                    const DescriptorPool& pool);
        
        const Mesh& getMesh() const { return *mesh_; }
        std::shared_ptr<const Mesh> getMeshShared() const { return mesh_; }
        std::shared_ptr<Material> getMaterialShared() const { return material_; }
        const vk::raii::Buffer& getInstanceBuffer() const { return instanceBuffer_->getBuffer(); }
        uint32_t getInstanceCount() const { return static_cast<uint32_t>(instanceDatas_.size()); }

    private:
        std::shared_ptr<const Mesh> mesh_;
        std::shared_ptr<Material> material_;
        std::vector<InstanceData>   instanceDatas_;
        std::unique_ptr<Buffer<InstanceData>> instanceBuffer_ = nullptr;
};