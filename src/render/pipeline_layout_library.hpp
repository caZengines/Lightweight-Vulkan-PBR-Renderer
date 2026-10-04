#pragma once

#include "render_context.hpp"

#include <bit>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace render {

struct PushConstantRangeSpec {
    vk::ShaderStageFlags stages{};   
    uint32_t             offset = 0;
    uint32_t             size   = 0;
    friend constexpr bool operator==(const PushConstantRangeSpec&, const PushConstantRangeSpec&) = default;
};

struct PipelineLayoutSpec {
    std::vector<vk::DescriptorSetLayout> setLayouts;
    std::vector<PushConstantRangeSpec>   pushConstants;

};
inline bool operator==(const PipelineLayoutSpec& a, const PipelineLayoutSpec& b) {
    return a.setLayouts    == b.setLayouts &&
           a.pushConstants == b.pushConstants;
}

} // namespace render

namespace std {
template<> 
struct hash<render::PipelineLayoutSpec> {
    static void combine(uint64_t& seed, uint64_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    size_t operator()(render::PipelineLayoutSpec const& s) const noexcept {
        uint64_t h = 0;
        for(auto sl : s.setLayouts) combine(h, std::bit_cast<uint64_t>(
                                                                                static_cast<VkDescriptorSetLayout>(sl)));
        for (const auto& pc : s.pushConstants) {
            combine(h, static_cast<uint32_t>(pc.stages));
            combine(h, pc.offset);
            combine(h, pc.size);
        }
        return h;
    }
};
} // namespace std

namespace render {

class DescriptorSetLayoutLibrary;

class PipelineLayout final {
    public:
        PipelineLayout(const PipelineLayout&) = delete;
        PipelineLayout& operator=(const PipelineLayout&) = delete;
        PipelineLayout(PipelineLayout&&) noexcept = default;
        PipelineLayout& operator=(PipelineLayout&&) noexcept = default;

        [[nodiscard]] vk::PipelineLayout getHandle() const noexcept { return *layout_; }
        [[nodiscard]] const PipelineLayoutSpec& spec() const { return spec_; }

    private:
        explicit PipelineLayout(RenderContext& rct, PipelineLayoutSpec layoutSpec);
        vk::raii::PipelineLayout                  layout_ = nullptr;
        PipelineLayoutSpec                        spec_; 

        friend class PipelineLayoutLibrary;
};

class PipelineLayoutLibrary final {
    public:
        explicit PipelineLayoutLibrary(RenderContext& rct, const DescriptorSetLayoutLibrary& layoutLibrary);

        PipelineLayoutLibrary(const PipelineLayoutLibrary&) = delete;
        PipelineLayoutLibrary& operator=(const PipelineLayoutLibrary&) = delete;
        PipelineLayoutLibrary(PipelineLayoutLibrary&&) noexcept = delete;
        PipelineLayoutLibrary& operator=(PipelineLayoutLibrary&&) noexcept = delete;

        // Query (create if non-existent)
        [[nodiscard]] const PipelineLayout& getFor(
            std::span<const std::string_view> paths,
            std::span<const PushConstantRangeSpec> pcs = {});
        // only query
        [[nodiscard]] const PipelineLayout* findRaw(const PipelineLayoutSpec& spec) const;

        bool   empty() const noexcept { return cache_.empty(); }
        void   clear() { cache_.clear(); }

    private:
        RenderContext&                                       rct_;
        const DescriptorSetLayoutLibrary&                    dsl_;
        struct Key {
            std::vector<std::string>            paths; 
            std::vector<PushConstantRangeSpec>  pcs;
            friend bool operator==(const Key&, const Key&) = default;
        };
        struct KeyHash { std::size_t operator()(const Key&) const noexcept; };
        std::unordered_map<Key, std::unique_ptr<PipelineLayout>, KeyHash>  cache_; 
};

} //namespace render