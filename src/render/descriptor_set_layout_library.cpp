#include "render/descriptor_set_layout_library.hpp"
#include "resource/shader_library.hpp"
#include "vulkan/vulkan.hpp"
#include <stdexcept>

namespace render {

// ---------------------------------------------------------------------------
// Ownership model
//   emptyLayout_     : library-owned, one instance.
//   globalLayout_    : library-owned, one instance (set 0).
//   uniqueLayouts_   : library-owned, deduplicated by fingerprint.
//   cache_[key]      : per-shader-combination views into the above.
//
// The global layout is fixed at construction.  Nothing in this class mutates
// it afterwards, so no cache invalidation is ever needed.
//
// Thread-safety
//   Not thread-safe.  layoutSetFor() mutates cache_ and uniqueLayouts_.
// ---------------------------------------------------------------------------

DescriptorSetLayoutLibrary::DescriptorSetLayoutLibrary(RenderContext& rct, 
                                                       const resource::ShaderLibrary& shaderLibrary,
                                                       std::span<const rhi::DescriptorBinding> globalBindings)
    : rct_(rct), library_(shaderLibrary) 
{
    vk::DescriptorSetLayoutCreateInfo ci{};
    emptyLayout_ = vk::raii::DescriptorSetLayout(rct_.device, ci);
    if (globalBindings.empty()) return;
    rhi::DescriptorBindingTable bindings(globalBindings.begin(), globalBindings.end());
    setGlobalLayout(bindings);
}

void DescriptorSetLayoutLibrary::setGlobalLayout(const rhi::DescriptorBindingTable& bindings) {
    auto sorted = bindings;
    std::ranges::sort(sorted, {}, [](const rhi::DescriptorBinding& b) { return b.vk.binding; });

    for (auto& b : sorted) b.vk.setStageFlags(vk::ShaderStageFlagBits::eAll);
    globalLayout_   = createLayout(sorted);
    globalBindings_ = std::move(sorted);
    hasGlobalLayout_ = true;
}

DescriptorSetLayoutLibrary::cacheKey DescriptorSetLayoutLibrary::makeCacheKey(
            std::span<const std::string_view> spirvPaths) {
    cacheKey key;
    key.reserve(spirvPaths.size());
    for(const auto& path : spirvPaths) key.emplace_back(path);
    std::ranges::sort(key);
    key.erase(std::unique(key.begin(), key.end()), key.end());

    return key;
}

DescriptorSetLayoutLibrary::LayoutFingerprint DescriptorSetLayoutLibrary::fingerprint(
            const rhi::DescriptorBindingTable& bindings) {
    LayoutFingerprint fp{};
    fp.reserve(bindings.size());
    for(const auto& b : bindings) {
        fp.emplace_back(BindingFingerprint{
            b.vk.binding, b.vk.descriptorType, b.vk.descriptorCount, b.vk.stageFlags
        });
    }
    std::ranges::sort(fp, {}, &BindingFingerprint::binding);
    return fp;
}

const vk::raii::DescriptorSetLayout& DescriptorSetLayoutLibrary::internLayout(
            const rhi::DescriptorBindingTable& bindings) const {
    LayoutFingerprint fp = fingerprint(bindings);

    auto it = uniqueLayouts_.find(fp);
    if(it != uniqueLayouts_.end()) return it->second;

    auto [inserted, _] = uniqueLayouts_.emplace(std::move(fp), createLayout(bindings));
    return inserted->second;
}

const LayoutSet& DescriptorSetLayoutLibrary::layoutSetFor(std::span<const std::string_view> spirvPaths) const {
    cacheKey key = makeCacheKey(spirvPaths);

    if(auto it = cache_.find(key) ; it != cache_.end())  return it->second;

    LayoutSet cached = buildCachedLayoutSet(key);
    auto [it, _] = cache_.emplace(std::move(key), std::move(cached));
    return it->second;
}

LayoutSet DescriptorSetLayoutLibrary::buildCachedLayoutSet(const cacheKey& paths) const {
    LayoutSet cached{};

    auto tables = buildReflectedTables(paths);

    if (hasGlobalLayout_) {
        // Set 0 is the hand-made layout.  Its entries have empty names; the
        // renderer writes it by number.
        if (tables.empty()) tables.resize(1);
        tables[0] = globalBindings_;
    }
    cached.bindingTables = std::move(tables);
    const size_t n = cached.bindingTables.size();
    cached.bySetIndex.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        const auto& bindings = cached.bindingTables[i];

        if (i == 0 && hasGlobalLayout_) {
            cached.bySetIndex.emplace_back(*globalLayout_);
            continue;
        }
        if (bindings.empty()) {
            cached.bySetIndex.emplace_back(*emptyLayout_);
            continue;
        }
        cached.bySetIndex.emplace_back(*internLayout(bindings));
    }

    return cached;
}

vk::raii::DescriptorSetLayout DescriptorSetLayoutLibrary::createLayout(
            const rhi::DescriptorBindingTable& bindings) const {
    const std::vector<vk::DescriptorSetLayoutBinding> vkBindings = rhi::vkBindingsOf(bindings);
    vk::DescriptorSetLayoutCreateInfo info{};
    info.setBindings(vkBindings);
    return vk::raii::DescriptorSetLayout(rct_.device, info);
}

rhi::DescriptorBindingTable globalSetBindings() {
    rhi::DescriptorBinding ubo{};
    ubo.vk.setBinding(0)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1)
          .setStageFlags(vk::ShaderStageFlagBits::eAll);   // the library forces eAll anyway
    rhi::DescriptorBindingTable table;
    table.emplace_back(std::move(ubo));
    return table;
}

std::vector<rhi::DescriptorBindingTable>
        DescriptorSetLayoutLibrary::buildReflectedTables(const cacheKey& paths) const {
    std::vector<rhi::DescriptorBindingTable> tables;

    for(const auto& path : paths) {
        for(const ReflectBinding& rb : library_.bindings(path)) {
            if(rb.stageFlags == vk::ShaderStageFlags{}) continue;

            if(rb.set >= tables.size()) tables.resize(rb.set + 1);
            auto& dst = tables[rb.set];

            auto it = std::ranges::find_if(dst, [&](const rhi::DescriptorBinding& b) {
                return b.vk.binding == rb.binding;
            });
            if(it == dst.end()) {
                rhi::DescriptorBinding binding{};
                binding.vk.setBinding(rb.binding)
                          .setDescriptorType(rb.descriptorType)
                          .setStageFlags(rb.stageFlags)
                          .setDescriptorCount(rb.count);
                binding.name = rb.name;
                dst.emplace_back(std::move(binding));
            }
            else {
                if(it->vk.descriptorType != rb.descriptorType) throw std::runtime_error("descriptor conflict at set=" + 
                      std::to_string(rb.set) +
                    " binding=" + std::to_string(rb.binding) +
                    " (shader: " + path + "): type mismatch");
                if(it->vk.descriptorCount != rb.count) throw std::runtime_error("descriptor conflict at set=" + 
                      std::to_string(rb.set) +
                    " binding=" + std::to_string(rb.binding) +
                    " (shader: " + path + "): count mismatch");
                // A binding shared by two modules must carry the same name in both.
                if(it->name != rb.name) throw std::runtime_error("descriptor conflict at set=" +
                      std::to_string(rb.set) +
                    " binding=" + std::to_string(rb.binding) +
                    " (shader: " + path + "): name mismatch ('" + it->name +
                    "' vs '" + rb.name + "')");
                it->vk.stageFlags |= rb.stageFlags;
            }
        }
    }

    for (auto& table : tables) {
        std::ranges::sort(table, {}, [](const rhi::DescriptorBinding& b) { return b.vk.binding; });
    }
    return tables;
}


} // namespace render
