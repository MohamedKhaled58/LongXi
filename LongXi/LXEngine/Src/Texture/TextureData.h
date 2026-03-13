#pragma once

#include <cstdint>
#include <vector>

#include "Core/Graphics/TextureFormat.h"

// =============================================================================
// TextureData — CPU-side decoded texture data
// Transient: created by TextureLoader, consumed by DX11Renderer::CreateTexture,
// then discarded
// =============================================================================

namespace LXEngine
{

using LXCore::TextureFormat;

struct TextureData
{
    uint32_t             Width;
    uint32_t             Height;
    TextureFormat        Format;
    std::vector<uint8_t> Pixels;
};

} // namespace LXEngine
