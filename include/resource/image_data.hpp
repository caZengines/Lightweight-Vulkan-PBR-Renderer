#pragma once
#include <cstdint>
#include <vector>

namespace resource {

// CPU-side image: RGBA8 pixel data + dimensions (Layer 2 discipline: pure
// data, no Vulkan/GPU types). GPU image/view creation happens later in
// ResourceRegistry — Texture/TextureGPU no longer create buffers themselves.
class ImageData {
    public:
        ImageData() = default;
        ImageData(std::vector<uint8_t> pixels, uint32_t width, uint32_t height)
            : pixels_(std::move(pixels)), width_(width), height_(height) {}

        const std::vector<uint8_t>& pixels() const { return pixels_; }
        uint32_t width()  const { return width_; }
        uint32_t height() const { return height_; }
        bool     empty()  const { return pixels_.empty(); }

    private:
        std::vector<uint8_t> pixels_;
        uint32_t             width_  = 0;
        uint32_t             height_ = 0;
};

}  // namespace resource
