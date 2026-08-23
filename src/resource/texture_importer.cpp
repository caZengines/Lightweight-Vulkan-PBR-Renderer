#include "resource/texture_importer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb_image.h"

#include <stdexcept>

namespace resource {

ImageData TextureImporter::load(const std::string& path) {
    int texWidth = 0, texHeight = 0, texChannel = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannel, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + path);
    }
    const size_t byteCount = static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4;
    std::vector<uint8_t> data(pixels, pixels + byteCount);
    stbi_image_free(pixels);
    return ImageData(std::move(data), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
}

}  // namespace resource
