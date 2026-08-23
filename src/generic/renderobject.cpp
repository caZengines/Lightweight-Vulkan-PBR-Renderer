#include "generic/renderobject.hpp"
#include "vma_allocator.hpp"

#include <stdexcept>

RenderObject::RenderObject(const resource::AssetHandle& mesh,
                           std::shared_ptr<Material> material,
                           const resource::ResourceRegistry& registry)
    : meshHandle_(mesh), material_(std::move(material))
{
    if (!mesh.valid()) {
        throw std::runtime_error("RenderObject: null mesh handle (asset not loaded)");
    }
    meshGPU_ = &registry.mesh(mesh);
}

void RenderObject::setInstances(VmaAllocator alloc, const std::vector<InstanceData>& instances,
                                resource::UploadQueue& queue) {
    instanceDatas_ = instances;
    Buffer<InstanceData>::CreateInfo instanceInfo;
    instanceInfo.size = sizeof(InstanceData) * instanceDatas_.size();
    instanceInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;

    instanceBuffer_ = std::make_unique<Buffer<InstanceData>>(alloc, instanceDatas_, instanceInfo, queue);
}

void RenderObject::initMaterialDescriptor(RenderContext& rct, const vk::DescriptorSetLayout& layout, const DescriptorPool& pool) {
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(pool.getDescriptorPool())
             .setDescriptorSetCount(1)
             .setSetLayouts(layout);
    material_->createDescriptorSet(rct, allocInfo);
}
