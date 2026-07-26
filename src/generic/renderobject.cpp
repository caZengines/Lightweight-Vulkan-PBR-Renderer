#include "generic/renderobject.hpp"
#include "vulkan/vulkan.hpp"

RenderObject::RenderObject(std::shared_ptr<const Mesh> mesh, std::shared_ptr<Material> material)
    : mesh_(mesh), material_(material) {}

void RenderObject::setInstances(const std::vector<InstanceData>& instances, CommandPool& cmdPool) {
    instanceDatas_ = instances;
    Buffer<InstanceData>::CreateInfo instanceInfo;
    instanceInfo.size = sizeof(InstanceData) * instanceDatas_.size();
    instanceInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
    instanceInfo.memProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    instanceBuffer_ = std::make_unique<Buffer<InstanceData>>(instanceDatas_, instanceInfo, cmdPool);
}

void RenderObject::initMaterialDescriptor(RenderContext& rct, const vk::DescriptorSetLayout& layout, const DescriptorPool& pool) {
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(pool.getDescriptorPool())
             .setDescriptorSetCount(1)
             .setSetLayouts(layout);
    material_->createDescriptorSet(rct, allocInfo);
}