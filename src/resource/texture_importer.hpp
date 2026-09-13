#pragma once

#include <cstddef>
#include <cstdint>
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

        // Decode an in-memory encoded image (PNG/JPEG/…) into RGBA8 CPU
        // pixels. Throws on failure.
        static ImageData loadFromMemory(const uint8_t* data, size_t byteCount);
};

}  // namespace resource
