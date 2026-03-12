#pragma once

#include <cstdint>

#include "Renderer/RendererTypes.h"
#include "Texture/TextureFormat.h"

// =============================================================================
// Texture — GPU texture metadata wrapper.
// Stores opaque renderer texture handle (backend encapsulated).
// GPU lifetime is renderer-owned; this object lifetime is cache-owned.
// =============================================================================

namespace LongXi
{

class Texture
{
public:
    ~Texture() = default;

    // Non-copyable
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    // Getters
    uint32_t GetWidth() const
    {
        return m_Width;
    }

    uint32_t GetHeight() const
    {
        return m_Height;
    }

    TextureFormat GetFormat() const
    {
        return m_Format;
    }

    const RendererTextureHandle& GetHandle() const
    {
        return m_Handle;
    }

private:
    // Private constructor — only TextureManager can create Texture instances
    Texture(RendererTextureHandle handle, uint32_t width, uint32_t height, TextureFormat format)
        : m_Handle(handle)
        , m_Width(width)
        , m_Height(height)
        , m_Format(format)
    {
    }

    RendererTextureHandle m_Handle;
    uint32_t              m_Width;
    uint32_t              m_Height;
    TextureFormat         m_Format;

    friend class TextureManager;
};

} // namespace LongXi
