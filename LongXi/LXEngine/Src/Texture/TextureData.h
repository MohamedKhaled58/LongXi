#pragma once

#include "Texture/TextureFormat.h"
#include <cstdint>
#include <vector>

// =============================================================================
// TextureData — CPU-side decoded texture data
// Transient: created by TextureLoader, consumed by DX11Renderer::CreateTexture,
// then discarded
// =============================================================================

namespace LongXi
{

struct TextureData
{
    uint32_t Width;
    uint32_t Height;
    TextureFormat Format;
    std::vector<uint8_t> Pixels;
};

} // namespace LongXi
