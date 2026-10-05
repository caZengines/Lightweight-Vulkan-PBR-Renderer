#include "render/pipeline_layout_library.hpp"
#include "render/descriptor_set_layout_library.hpp"

#include <utility>

namespace render {

PipelineLayout::PipelineLayout(RenderContext& rct, PipelineLayoutSpec layoutSpec) : spec_(std::move(layoutSpec))
{
    // setPushConstantRanges() / setSetLayouts() store pointers into the arrays
    // they are handed, so both must still be alive at the create call below.
    std::vector<vk::PushConstantRange> ranges;
    ranges.reserve(spec_.pushConstants.size());
    for(const auto& pc : spec_.pushConstants) {
        ranges.emplace_back(vk::PushConstantRange()
                            .setSize(pc.size)
                            .setOffset(pc.offset)
                            .setStageFlags(pc.stages)
        );
    }

    vk::PipelineLayoutCreateInfo layoutInfo{};
    if(!ranges.empty()) {
        layoutInfo.setPushConstantRanges(ranges);
    }
    // spec_ is the live copy; layoutSpec has been moved from.
    layoutInfo.setSetLayouts(spec_.setLayouts);
    layout_ = vk::raii::PipelineLayout(rct.device, layoutInfo);
}

PipelineLayoutLibrary::PipelineLayoutLibrary(RenderContext& rct, const DescriptorSetLayoutLibrary& layoutLibrary)
    : rct_(rct), dsl_(layoutLibrary) {}

namespace {
void normalizeKey(std::vector<std::string>& paths,
                  std::vector<PushConstantRangeSpec>& pcs) {
    std::ranges::sort(paths);
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    std::ranges::sort(pcs, [](const PushConstantRangeSpec& a,
                              const PushConstantRangeSpec& b) {
        return std::tuple{a.offset, a.size, static_cast<uint32_t>(a.stages)} <
               std::tuple{b.offset, b.size, static_cast<uint32_t>(b.stages)};
    });
}

} // namespace

size_t PipelineLayoutLibrary::KeyHash::operator()(const Key& k) const noexcept {
    auto combine = [](uint64_t& seed, uint64_t v) {
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };

    uint64_t h = 0;
    for (const auto& p : k.paths) {
        combine(h, static_cast<uint64_t>(std::hash<std::string>{}(p)));
    }
    for (const auto& pc : k.pcs) {
        combine(h, static_cast<uint32_t>(pc.stages));
        combine(h, pc.offset);
        combine(h, pc.size);
    }
    return static_cast<size_t>(h);
}

const PipelineLayout& PipelineLayoutLibrary::getFor(std::span<const std::string_view> paths,
            std::span<const PushConstantRangeSpec> pcs) {
    Key key{};
    key.paths.reserve(paths.size());
    for (const auto& p : paths) key.paths.emplace_back(p);
    key.pcs.assign(pcs.begin(), pcs.end());
    normalizeKey(key.paths, key.pcs);

    if(auto it = cache_.find(key); it != cache_.end()) {
        return *it->second;
    }

    const LayoutSet& ls = dsl_.layoutSetFor(paths);

    PipelineLayoutSpec spec{};
    spec.setLayouts = ls.bySetIndex;
    spec.pushConstants = key.pcs;

    auto layout = 
                std::unique_ptr<PipelineLayout>(new PipelineLayout(rct_, std::move(spec)));
    auto [it, _] = cache_.emplace(std::move(key), std::move(layout));
    return *it->second;
}

const PipelineLayout* PipelineLayoutLibrary::findRaw(const PipelineLayoutSpec& spec) const {
    for (const auto& [k, v] : cache_) {
        const auto& s = v->spec();
        if (s.setLayouts    == spec.setLayouts &&
            s.pushConstants == spec.pushConstants) {
            return v.get();
        }
    }
    return nullptr;
}

} // namespace render
