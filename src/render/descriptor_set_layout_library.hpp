#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include "render_context.hpp"
#include "vulkan/vulkan.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"

namespace resource {
    class ShaderLibrary;
}

namespace render {

struct LayoutSet {            
    std::vector<vk::DescriptorSetLayout> bySetIndex;
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindingTables;

    // Parallel to bindingTables: the reflected variable name of each binding.
    // vk::DescriptorSetLayoutBinding carries no name, so name-based writing
    // (rhi::DescriptorWriter) needs this side table.  Entries are empty for
    // hand-made layouts (set 0), whose bindings are only reachable by number.
    std::vector<std::vector<std::string>> bindingNames;
};

// The hand-made set-0 binding table.  Set 0 is the one set NOT derived from
// reflection: it is shared by every pipeline family and must stay the same
// object at index 0 (see docs/pipeline-descriptor-refactor-plan.md D4 / §8).
// Adding to set 0 (e.g. the light array) means adding here AND to the pool
// budget (render::PoolBudgetBuilder).  The library forces stageFlags to eAll,
// so they need not be set here.
[[nodiscard]] std::vector<vk::DescriptorSetLayoutBinding> globalSetBindings();

class DescriptorSetLayoutLibrary final {
    public:
        DescriptorSetLayoutLibrary(const DescriptorSetLayoutLibrary&) = delete;
        DescriptorSetLayoutLibrary& operator=(const DescriptorSetLayoutLibrary&) = delete;
        DescriptorSetLayoutLibrary(const DescriptorSetLayoutLibrary&&) = delete;
        DescriptorSetLayoutLibrary& operator=(const DescriptorSetLayoutLibrary&&) = delete;


        DescriptorSetLayoutLibrary(RenderContext& rct, 
                                   const resource::ShaderLibrary&,
                                   std::span<const vk::DescriptorSetLayoutBinding> globalBindings = {}
                                );
        void setGlobalLayout(const std::vector<vk::DescriptorSetLayoutBinding>&);

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

        // Reflection output for one call: the per-set binding tables plus the
        // parallel per-set variable names (vk::DescriptorSetLayoutBinding has no
        // name field, so names must travel beside the tables).
        struct ReflectedTables {
            std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings;
            std::vector<std::vector<std::string>>                    names;
        };

        RenderContext&                  rct_;
        const resource::ShaderLibrary&  library_;

        vk::raii::DescriptorSetLayout               emptyLayout_     = nullptr;
        vk::raii::DescriptorSetLayout               globalLayout_    = nullptr;

        std::vector<vk::DescriptorSetLayoutBinding> globalBindings_;
        bool                                        hasGlobalLayout_ = false;

        // NOTE: cache_ and uniqueLayouts_ are mutable but not thread-safe.
        // Concurrent calls to layoutSetFor() from multiple threads are UB.
        mutable std::map<LayoutFingerprint, vk::raii::DescriptorSetLayout> uniqueLayouts_;
        mutable std::map<cacheKey, LayoutSet> cache_;

        [[nodiscard]] static cacheKey makeCacheKey(
            std::span<const std::string_view> spirvPaths);

        [[nodiscard]] static LayoutFingerprint fingerprint(
            const std::vector<vk::DescriptorSetLayoutBinding>&);

        [[nodiscard]] const vk::raii::DescriptorSetLayout& internLayout(
            const std::vector<vk::DescriptorSetLayoutBinding>&) const;

        [[nodiscard]] vk::raii::DescriptorSetLayout createLayout(
            const std::vector<vk::DescriptorSetLayoutBinding>&) const;

        [[nodiscard]] ReflectedTables buildReflectedTables(const cacheKey&) const;

        [[nodiscard]] LayoutSet buildCachedLayoutSet(const cacheKey& Paths) const;
};

}  // namespace render
