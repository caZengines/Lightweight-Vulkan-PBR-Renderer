#pragma once

#include <string>

namespace resource {

class ImageData;

// stb_image-based CPU importer. Output is pure ImageData (RGBA8) — the GPU
// image/view is created later by ResourceRegistry, so the importer never
// touches Vulkan buffers (Phase 2: texture no longer creates buffers).
class TextureImporter {
    public:
        // Load an image file into RGBA8 CPU pixels. Throws on failure.
        static ImageData load(const std::string& path);
};

}  // namespace resource
