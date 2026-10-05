#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include "render_context.hpp"
#include "rhi/descriptor_binding.hpp"
#include "vulkan/vulkan.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"

namespace resource {
    class ShaderLibrary;
}

namespace render {

struct LayoutSet {            
    std::vector<vk::DescriptorSetLayout> bySetIndex;
    // One table per set: index i describes set i.  Each entry pairs the
    // vk::DescriptorSetLayoutBinding with its reflected variable name.
    // Entries for hand-made layouts (set 0) have an empty name: those bindings
    // are reachable by number only.
    std::vector<rhi::DescriptorBindingTable> bindingTables;
};


// Adding to set 0 (e.g. the light array) means adding here AND to the pool
// budget (render::PoolBudgetBuilder).  The library forces stageFlags to eAll,
// so they need not be set here.  Names left empty = written by number.
[[nodiscard]] rhi::DescriptorBindingTable globalSetBindings();

class DescriptorSetLayoutLibrary final {
    public:
        DescriptorSetLayoutLibrary(const DescriptorSetLayoutLibrary&) = delete;
        DescriptorSetLayoutLibrary& operator=(const DescriptorSetLayoutLibrary&) = delete;
        DescriptorSetLayoutLibrary(const DescriptorSetLayoutLibrary&&) = delete;
        DescriptorSetLayoutLibrary& operator=(const DescriptorSetLayoutLibrary&&) = delete;


        DescriptorSetLayoutLibrary(RenderContext& rct, 
                                   const resource::ShaderLibrary&,
                                   std::span<const rhi::DescriptorBinding> globalBindings = {}
                                );
        void setGlobalLayout(const rhi::DescriptorBindingTable&);

        [[nodiscard]] const LayoutSet& layoutSetFor(std::span<const std::string_view> spirvPaths) const;

        [[nodiscard]] const vk::DescriptorSetLayout& emptyLayout() const { return *emptyLayout_; }
        [[nodiscard]] const vk::DescriptorSetLayout& globalLayout() const { 
            assert(hasGlobalLayout_);
            return *globalLayout_; 
        }
        [[nodiscard]] bool hasGlobalLayout() const noexcept { return hasGlobalLayout_; }

    private:
        using cacheKey = std::vector<std::string>;
        struct BindingFingerprint {
            uint32_t             binding = 0;
            vk::DescriptorType   type{};
            uint32_t             count   = 0;
            vk::ShaderStageFlags stages{};

            friend bool operator==(const BindingFingerprint&, const BindingFingerprint&) = default;
            friend bool operator<(const BindingFingerprint& a, const BindingFingerprint& b) {
                if (a.binding != b.binding) return a.binding < b.binding;
                if (a.type    != b.type)    return static_cast<uint32_t>(a.type)   < static_cast<uint32_t>(b.type);
                if (a.count   != b.count)   return a.count   < b.count;
                return static_cast<uint32_t>(a.stages) < static_cast<uint32_t>(b.stages);
            }
        };
        using LayoutFingerprint = std::vector<BindingFingerprint>; // sorted by binding number

        RenderContext&                  rct_;
        const resource::ShaderLibrary&  library_;

        vk::raii::DescriptorSetLayout               emptyLayout_     = nullptr;
        vk::raii::DescriptorSetLayout               globalLayout_    = nullptr;

        rhi::DescriptorBindingTable                 globalBindings_;
        bool                                        hasGlobalLayout_ = false;

        // NOTE: cache_ and uniqueLayouts_ are mutable but not thread-safe.
        // Concurrent calls to layoutSetFor() from multiple threads are UB.
        mutable std::map<LayoutFingerprint, vk::raii::DescriptorSetLayout> uniqueLayouts_;
        mutable std::map<cacheKey, LayoutSet> cache_;

        [[nodiscard]] static cacheKey makeCacheKey(
            std::span<const std::string_view> spirvPaths);

        [[nodiscard]] static LayoutFingerprint fingerprint(
            const rhi::DescriptorBindingTable&);

        [[nodiscard]] const vk::raii::DescriptorSetLayout& internLayout(
            const rhi::DescriptorBindingTable&) const;

        [[nodiscard]] vk::raii::DescriptorSetLayout createLayout(
            const rhi::DescriptorBindingTable&) const;

        // One table per set, each binding paired with its reflected name.
        [[nodiscard]] std::vector<rhi::DescriptorBindingTable> buildReflectedTables(
            const cacheKey&) const;

        [[nodiscard]] LayoutSet buildCachedLayoutSet(const cacheKey& Paths) const;
};

}  // namespace render
