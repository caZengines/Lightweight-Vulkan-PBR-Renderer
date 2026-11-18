#include "scene/primitive_batching.hpp"
#include "render/frame/render_item.hpp"
#include "render/material/material.hpp"

#include <algorithm>
#include <numeric>
#include <cstdint> 
#include <utility>
#include <span>
#include <vector>

namespace  {

template<class Less>
void batchByKey(std::span<render::RenderItem> items,
                Less less,
                std::vector<render::RenderItem>& scratch,
                std::vector<uint32_t>&            order,
                std::vector<std::pair<uint32_t, uint32_t>>& runs) 
{
    const size_t n = items.size();
    if(n < 2) return;

    auto equal = [&](const render::RenderItem& a, const render::RenderItem& b) {
        return !less(a, b) && !less(b, a);
    };

    // Indirect sorting: order [i] is 'the original index corresponding to the i-th sorted position'.
    order.resize(n);
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::stable_sort(order, [&](uint32_t a, uint32_t b) {
        return less(items[a], items[b]);
    });
    
    // Adjacent with the same key, record the [begin, end) interval within the order.
    runs.clear();
    for(uint32_t i = 0; i < n; ) {
        uint32_t j = i + 1;
        while (j < n && equal(items[order[i]], items[order[j]])) ++j;
        runs.emplace_back(i, j);
        i = j;
    }

    // sort by batch size.
    std::ranges::stable_sort(runs, [](const auto& a, const auto& b) {
        return (a.second - a.first) > (b.second - b.first);
    });

    scratch.clear();
    scratch.reserve(n);
    for(const auto& r : runs) {
        for(uint32_t k = r.first ; k < r.second ; ++k) {
            scratch.push_back(items[order[k]]);
        }
    }

    std::ranges::copy(scratch, items.begin());
}

} // namespace 

namespace scene {

void batchRenderItems(std::span<render::RenderItem> items) {
    if(items.size() < 2) return;

    std::vector<render::RenderItem>            scratch;
    std::vector<uint32_t>                      order;
    std::vector<std::pair<uint32_t, uint32_t>> runs;
    scratch.reserve(items.size());
    order.reserve(items.size());

    batchByKey(items, 
        [](const render::RenderItem& a, const render::RenderItem& b) {
            return a.material->pipelineKey() < b.material->pipelineKey();
        }, 
        scratch, order, runs);

    size_t i = 0;
    while(i < items.size()) {
        size_t j = i + 1;
        while(j < items.size() && 
                (items[j].material->pipelineKey() == items[i].material->pipelineKey())) {
            ++j;
        }
        batchByKey(items.subspan(i, j-i), 
            [](const render::RenderItem& a, const render::RenderItem& b) {
                return a.material->id() < b.material->id();
        }, 
        scratch, order, runs);

        i = j;
    }
}

}  // namespace scene
