#pragma once

#include <vector>

#include "Texture/TextureData.h"

// =============================================================================
// TextureLoader — Static texture decoder
// Loads DDS and TGA files into TextureData structures
// Zero DX11 dependencies — pure CPU-side decoding
// =============================================================================

namespace LXEngine
{

class TextureLoader
{
public:
    // Load DDS file data (DXT1, DXT3, DXT5, RGBA8)
    // Returns true on success, false on failure (out is unchanged on failure)
    static bool LoadDDS(const std::vector<uint8_t>& data, TextureData& out);

    // Load TGA file data (24-bit or 32-bit, type 2 uncompressed or type 10 RLE)
    // Always outputs RGBA8 format (24-bit expanded with 0xFF alpha)
    // Returns true on success, false on failure (out is unchanged on failure)
    static bool LoadTGA(const std::vector<uint8_t>& data, TextureData& out);

    // Load MSK file data (8-bit grayscale alpha mask)
    // Returns raw 8-bit grayscale values, width, and height in the provided vectors
    // Returns true on success, false on failure
    static bool LoadMSK(const std::vector<uint8_t>& data, std::vector<uint8_t>& outAlphaData, uint32_t& outWidth, uint32_t& outHeight);

    // Apply MSK alpha mask to RGBA texture data
    // Replaces the alpha channel of textureData with values from alphaMaskData
    // Both must have the same dimensions
    static void ApplyMSKMask(TextureData& textureData, const std::vector<uint8_t>& alphaMaskData);

    // Non-instantiable
    TextureLoader()  = delete;
    ~TextureLoader() = delete;
};

} // namespace LXEngine
