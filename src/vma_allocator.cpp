#define VMA_IMPLEMENTATION
#include "vma_allocator.hpp"

VmaContext::VmaContext(vk::PhysicalDevice pd, vk::Device device, vk::Instance instance) {
    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.physicalDevice = pd;
    allocInfo.device         = device;
    allocInfo.instance       = instance;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocInfo, &vmaAllocator_);
}
VmaContext::~VmaContext() {
    vmaDestroyAllocator(vmaAllocator_); 
}

// VmaBuffer
VmaBuffer::VmaBuffer(VmaAllocator alloc, const VkBufferCreateInfo& bufferCI, 
                     const VmaAllocationCreateInfo& allocCI)
    : allocator_(alloc)
{
    vmaCreateBuffer(allocator_, &bufferCI, &allocCI, &buffer_, &allocation_, nullptr);
}

void* VmaBuffer::map() {
    void* data;
    vmaMapMemory(allocator_, allocation_, &data);
    return data;
}
void VmaBuffer::unmap() {
    vmaUnmapMemory(allocator_, allocation_);
}
void* VmaBuffer::mappedData() {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(allocator_, allocation_, &info);
    return info.pMappedData;
}
void VmaBuffer::destroy() {
    if(buffer_ != VK_NULL_HANDLE && allocator_) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }
}

// VmaImage
VmaImage::VmaImage(VmaAllocator alloc, const VkImageCreateInfo& imageCI,
                   const VmaAllocationCreateInfo& allocCI)
    : allocator_(alloc)
{
    vmaCreateImage(allocator_, &imageCI, &allocCI, &image_, &allocation_, nullptr);
}

void VmaImage::destroy() {
    if(image_ != VK_NULL_HANDLE && allocator_) {
        vmaDestroyImage(allocator_, image_, allocation_);
        image_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }
}
