#pragma once

#include <cstdint>

// =============================================================================
// TextureFormat — Texture pixel format enumeration (moved from LXEngine)
// Zero DX11 dependencies — pure data type
// =============================================================================

namespace LXCore
{

enum class TextureFormat : uint32_t
{
    RGBA8, // Uncompressed; 4 bytes/pixel
    DXT1,  // BC1 compressed; 8 bytes/4×4 block
    DXT3,  // BC2 compressed; 16 bytes/4×4 block
    DXT5   // BC3 compressed; 16 bytes/4×4 block
};

} // namespace LXCore
