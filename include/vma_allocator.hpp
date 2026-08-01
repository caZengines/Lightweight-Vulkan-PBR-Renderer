#pragma once
#include "extern/vk_mem_alloc.h"
#include "vulkan/vulkan.hpp"

class VmaContext {
    public:
        VmaContext(vk::PhysicalDevice pd, vk::Device device, vk::Instance instance);
        ~VmaContext();

        // Ban copy and move
        VmaContext(const VmaContext&) = delete;
        VmaContext& operator=(const VmaContext&) = delete;
        VmaContext(VmaContext&&) = delete;
        VmaContext& operator=(VmaContext&&) = delete;

        VmaAllocator* getAllocator() { return &vmaAllocator_; }

    private:
        VmaAllocator vmaAllocator_ = nullptr;
};

class VmaBuffer {
    public:
    VmaBuffer() = default;

    VmaBuffer(VmaAllocator* alloc, const VkBufferCreateInfo& bufferCI, 
              const VmaAllocationCreateInfo& allocCI);
    ~VmaBuffer() { destroy(); }

    //Ban copying
    VmaBuffer(const VmaBuffer&) = delete;
    VmaBuffer& operator=(const VmaBuffer&) = delete;

    VmaBuffer(VmaBuffer&& other) noexcept
        : buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE))
        , allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE))
        , allocator_(std::exchange(other.allocator_, nullptr)) {}
    VmaBuffer& operator=(VmaBuffer&& other) noexcept {
        if(this != &other) {
            destroy();
            buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
            allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
            allocator_ = std::exchange(other.allocator_, nullptr);
        }
        return *this;
    }

    const VkBuffer& getHandle() const { return buffer_; } 

    void* map();
    void unmap();

    /// For MAPPED_BIT allocations — returns the persistently-mapped pointer
    /// without incrementing the internal map counter.
    /// MUST only be used on allocations created with VMA_ALLOCATION_CREATE_MAPPED_BIT.
    void* mappedData();

    private:
        VkBuffer      buffer_     = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator* allocator_  = nullptr;

        void destroy();
};

class VmaImage {
    public:
        VmaImage() = default;

        VmaImage(VmaAllocator* alloc, const VkImageCreateInfo& imageCI,
                 const VmaAllocationCreateInfo& allocCI);
        ~VmaImage() { destroy(); }

        //Ban copying
        VmaImage(const VmaImage&) = delete;
        VmaImage& operator=(const VmaImage&) = delete;

        VmaImage(VmaImage&& other) noexcept
            : image_(std::exchange(other.image_, VK_NULL_HANDLE))
            , allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE))
            , allocator_(std::exchange(other.allocator_, nullptr)) {}
        VmaImage& operator=(VmaImage&& other) noexcept {
            if(this != &other) {
                destroy();
                image_ = std::exchange(other.image_, VK_NULL_HANDLE);
                allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
                allocator_ = std::exchange(other.allocator_, nullptr);
            }
            return *this;
        }

        const VkImage& getHandle() const { return image_; }
    private:
        VkImage       image_      = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator* allocator_  = nullptr;

        void destroy();
};