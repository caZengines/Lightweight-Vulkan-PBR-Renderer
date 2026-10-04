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
                                                       std::span<const vk::DescriptorSetLayoutBinding> globalBindings)
    : rct_(rct), library_(shaderLibrary) 
{
    vk::DescriptorSetLayoutCreateInfo ci{};
    emptyLayout_ = vk::raii::DescriptorSetLayout(rct_.device, ci);
    if (globalBindings.empty()) return;
    std::vector<vk::DescriptorSetLayoutBinding> bindings(globalBindings.begin(), globalBindings.end());
    setGlobalLayout(bindings);
}

void DescriptorSetLayoutLibrary::setGlobalLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings) {
    auto sorted = bindings;
    std::ranges::sort(sorted, {}, &vk::DescriptorSetLayoutBinding::binding);

    for (auto& b : sorted) b.setStageFlags(vk::ShaderStageFlagBits::eAll);
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
            const std::vector<vk::DescriptorSetLayoutBinding>& bindings) {
    LayoutFingerprint fp{};
    fp.reserve(bindings.size());
    for(const auto& b : bindings) {
        fp.emplace_back(BindingFingerprint{
            b.binding, b.descriptorType, b.descriptorCount, b.stageFlags
        });
    }
    std::ranges::sort(fp, {}, &BindingFingerprint::binding);
    return fp;
}

const vk::raii::DescriptorSetLayout& DescriptorSetLayoutLibrary::internLayout(
            const std::vector<vk::DescriptorSetLayoutBinding>& bindings) const {
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
        if (tables.bindings.empty()) {
            tables.bindings.resize(1);
            tables.names.resize(1);
        }
        // preserve reserved-but-unused bindings
        tables.bindings[0] = globalBindings_;
        // Set 0 is hand-made from a nameless vk::DescriptorSetLayoutBinding table,
        // so it has no names: its bindings are only reachable by number.
        tables.names[0].assign(globalBindings_.size(), std::string{});
    }
    cached.bindingTables = std::move(tables.bindings);
    cached.bindingNames  = std::move(tables.names);
    cached.bindingNames.resize(cached.bindingTables.size());   // keep the two parallel
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
            const std::vector<vk::DescriptorSetLayoutBinding>& bindings) const {
    vk::DescriptorSetLayoutCreateInfo info{};
    info.setBindings(bindings);
    return vk::raii::DescriptorSetLayout(rct_.device, info);
}

std::vector<vk::DescriptorSetLayoutBinding> globalSetBindings() {
    // Set 0 today: the per-frame uniform buffer (binding 0), written every frame
    // by the renderer.  The light array will land here next.
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    vk::DescriptorSetLayoutBinding ubo{};
    ubo.setBinding(0)
       .setDescriptorType(vk::DescriptorType::eUniformBuffer)
       .setDescriptorCount(1)
       .setStageFlags(vk::ShaderStageFlagBits::eAll);   // the library forces eAll anyway
    bindings.emplace_back(ubo);
    return bindings;
}

DescriptorSetLayoutLibrary::ReflectedTables
        DescriptorSetLayoutLibrary::buildReflectedTables(const cacheKey& paths) const {
    // Names must stay aligned with bindings through the merge and the sort, so
    // work on pairs and split at the end.
    std::vector<std::vector<std::pair<vk::DescriptorSetLayoutBinding, std::string>>> tables;

    for(const auto& path : paths) {
        for(const ReflectBinding& rb : library_.bindings(path)) {
            if(rb.stageFlags == vk::ShaderStageFlags{}) continue;

            if(rb.set >= tables.size()) tables.resize(rb.set + 1);
            auto& dst = tables[rb.set];

            auto it = std::ranges::find_if(dst, [&](const auto& b) {
                return b.first.binding == rb.binding;
            });
            if(it == dst.end()) {
                vk::DescriptorSetLayoutBinding binding{};
                binding.setBinding(rb.binding)
                       .setDescriptorType(rb.descriptorType)
                       .setStageFlags(rb.stageFlags)
                       .setDescriptorCount(rb.count);
                dst.emplace_back(binding, rb.name);
            }
            else {
                if(it->first.descriptorType != rb.descriptorType) throw std::runtime_error("descriptor conflict at set=" + 
                      std::to_string(rb.set) +
                    " binding=" + std::to_string(rb.binding) +
                    " (shader: " + path + "): type mismatch");
                if(it->first.descriptorCount != rb.count) throw std::runtime_error("descriptor conflict at set=" + 
                      std::to_string(rb.set) +
                    " binding=" + std::to_string(rb.binding) +
                    " (shader: " + path + "): count mismatch");
                it->first.stageFlags |= rb.stageFlags;
            }
        }
    }

    ReflectedTables out;
    out.bindings.resize(tables.size());
    out.names.resize(tables.size());
    for (size_t i = 0; i < tables.size(); ++i) {
        std::ranges::sort(tables[i], {}, [](const auto& p) { return p.first.binding; });
        out.bindings[i].reserve(tables[i].size());
        out.names[i].reserve(tables[i].size());
        for (auto& [binding, name] : tables[i]) {
            out.bindings[i].emplace_back(binding);
            out.names[i].emplace_back(std::move(name));
        }
    }
    return out;
}


} // namespace render