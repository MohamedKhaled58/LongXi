#pragma once

#include "Texture/TextureData.h"
#include <vector>

// =============================================================================
// TextureLoader — Static texture decoder
// Loads DDS and TGA files into TextureData structures
// Zero DX11 dependencies — pure CPU-side decoding
// =============================================================================

namespace LongXi
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

    // Non-instantiable
    TextureLoader() = delete;
    ~TextureLoader() = delete;
};

} // namespace LongXi
