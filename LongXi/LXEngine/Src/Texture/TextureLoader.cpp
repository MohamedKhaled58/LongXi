#include "Texture/TextureLoader.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "Core/Graphics/TextureFormat.h"
#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

using LXCore::TextureFormat;

namespace
{
bool CheckedMul(size_t a, size_t b, size_t& out)
{
    if (a != 0 && b > (std::numeric_limits<size_t>::max() / a))
        return false;
    out = a * b;
    return true;
}
} // namespace

// =============================================================================
// DDS Structures
// =============================================================================

#pragma pack(push, 1)

struct DDS_PIXELFORMAT
{
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER
{
    uint32_t        dwSize;
    uint32_t        dwFlags;
    uint32_t        dwHeight;
    uint32_t        dwWidth;
    uint32_t        dwPitchOrLinearSize;
    uint32_t        dwDepth;
    uint32_t        dwMipMapCount;
    uint32_t        dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        dwCaps;
    uint32_t        dwCaps2;
    uint32_t        dwCaps3;
    uint32_t        dwCaps4;
    uint32_t        dwReserved2;
};

#pragma pack(pop)

// DDS flags
#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_LINEARSIZE 0x00080000
#define DDSD_DEPTH 0x00800000

// DDS pixel format flags
#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_ALPHA 0x00000002
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB 0x00000040
#define DDPF_YUV 0x00000200
#define DDPF_LUMINANCE 0x00020000

// FourCC codes
#define FOURCC_DXT1 0x31545844 // "DXT1"
#define FOURCC_DXT3 0x33545844 // "DXT3"
#define FOURCC_DXT5 0x35545844 // "DXT5"

// =============================================================================
// TGA Structures
// =============================================================================

#pragma pack(push, 1)

struct TGA_HEADER
{
    uint8_t  idLength;
    uint8_t  colorMapType;
    uint8_t  imageType;
    uint16_t colorMapOrigin;
    uint16_t colorMapLength;
    uint8_t  colorMapEntrySize;
    uint16_t xOrigin;
    uint16_t yOrigin;
    uint16_t width;
    uint16_t height;
    uint8_t  bitDepth;
    uint8_t  imageDescriptor;
};

#pragma pack(pop)

// TGA image types
#define TGA_TYPE_UNCOMPRESSED_RGB 2
#define TGA_TYPE_RUN_LENGTH_ENCODED 10

// =============================================================================
// LoadDDS
// =============================================================================

bool TextureLoader::LoadDDS(const std::vector<uint8_t>& data, TextureData& out)
{
    // Validate minimum size (4 magic + 124 header = 128 bytes minimum)
    if (data.size() < 128)
    {
        LX_ENGINE_ERROR("[Texture] DDS file too small: {} bytes", data.size());
        return false;
    }

    // Validate magic "DDS " (bytes 0-3)
    if (data[0] != 'D' || data[1] != 'D' || data[2] != 'S' || data[3] != ' ')
    {
        LX_ENGINE_ERROR("[Texture] DDS magic number mismatch");
        return false;
    }

    // Parse header at offset 4
    DDS_HEADER header;
    std::memcpy(&header, &data[4], sizeof(DDS_HEADER));

    // Extract dimensions
    uint32_t width  = header.dwWidth;
    uint32_t height = header.dwHeight;

    if (width == 0 || height == 0)
    {
        LX_ENGINE_ERROR("[Texture] DDS invalid dimensions: {}x{}", width, height);
        return false;
    }

    // Determine format from pixel format
    TextureFormat format;
    uint32_t      blockSize = 0; // Bytes per block (for compressed formats)

    if (header.ddspf.dwFlags & DDPF_FOURCC)
    {
        // Compressed format
        uint32_t fourCC = header.ddspf.dwFourCC;

        if (fourCC == FOURCC_DXT1)
        {
            format    = TextureFormat::DXT1;
            blockSize = 8;
        }
        else if (fourCC == FOURCC_DXT3)
        {
            format    = TextureFormat::DXT3;
            blockSize = 16;
        }
        else if (fourCC == FOURCC_DXT5)
        {
            format    = TextureFormat::DXT5;
            blockSize = 16;
        }
        else
        {
            LX_ENGINE_ERROR("[Texture] DDS unsupported FOURCC: 0x{:08X}", fourCC);
            return false;
        }
    }
    else if ((header.ddspf.dwFlags & DDPF_RGB) && (header.ddspf.dwFlags & DDPF_ALPHAPIXELS) && header.ddspf.dwRGBBitCount == 32)
    {
        // Uncompressed RGBA8
        format = TextureFormat::RGBA8;
    }
    else
    {
        LX_ENGINE_ERROR("[Texture] DDS unsupported pixel format");
        return false;
    }

    // Compute pixel data size
    size_t pixelDataSize = 0;

    if (format == TextureFormat::RGBA8)
    {
        // Uncompressed: width * height * 4 bytes
        size_t texelCount = 0;
        if (!CheckedMul(static_cast<size_t>(width), static_cast<size_t>(height), texelCount) ||
            !CheckedMul(texelCount, size_t(4), pixelDataSize))
        {
            LX_ENGINE_ERROR("[Texture] DDS dimensions overflow size calculation: {}x{}", width, height);
            return false;
        }
    }
    else
    {
        // Compressed (DXT): block-row formula
        // Block rows = max(1, (height + 3) / 4)
        // Block cols = max(1, (width + 3) / 4)
        size_t blockRows = (height + 3) / 4;
        if (blockRows == 0)
            blockRows = 1;
        size_t blockCols = (width + 3) / 4;
        if (blockCols == 0)
            blockCols = 1;

        size_t blockCount = 0;
        if (!CheckedMul(blockRows, blockCols, blockCount) || !CheckedMul(blockCount, static_cast<size_t>(blockSize), pixelDataSize))
        {
            LX_ENGINE_ERROR("[Texture] DDS compressed size overflow: {}x{}", width, height);
            return false;
        }
    }

    // Validate we have enough data
    if (data.size() < 128 + pixelDataSize)
    {
        LX_ENGINE_ERROR("[Texture] DDS truncated: expected {} pixel bytes, have {}", pixelDataSize, data.size() - 128);
        return false;
    }

    // Copy pixel data from offset 128
    out.Width  = width;
    out.Height = height;
    out.Format = format;
    out.Pixels.assign(data.begin() + 128, data.begin() + 128 + pixelDataSize);

    return true;
}

// =============================================================================
// LoadTGA
// =============================================================================

bool TextureLoader::LoadTGA(const std::vector<uint8_t>& data, TextureData& out)
{
    // Validate minimum size (18-byte header)
    if (data.size() < sizeof(TGA_HEADER))
    {
        LX_ENGINE_ERROR("[Texture] TGA file too small: {} bytes", data.size());
        return false;
    }

    // Parse header
    TGA_HEADER header;
    std::memcpy(&header, data.data(), sizeof(TGA_HEADER));

    // Extract dimensions (little-endian)
    uint32_t width     = header.width;
    uint32_t height    = header.height;
    bool     topOrigin = (header.imageDescriptor & 0x20) != 0;

    if (width == 0 || height == 0)
    {
        LX_ENGINE_ERROR("[Texture] TGA invalid dimensions: {}x{}", width, height);
        return false;
    }

    // Validate image type
    if (header.imageType != TGA_TYPE_UNCOMPRESSED_RGB && header.imageType != TGA_TYPE_RUN_LENGTH_ENCODED)
    {
        LX_ENGINE_ERROR("[Texture] TGA unsupported image type: {}", header.imageType);
        return false;
    }

    // Validate bit depth
    if (header.bitDepth != 24 && header.bitDepth != 32)
    {
        LX_ENGINE_ERROR("[Texture] TGA unsupported bit depth: {}", header.bitDepth);
        return false;
    }

    // Calculate pixel data offset (skip ID and color map)
    size_t pixelOffset = sizeof(TGA_HEADER) + header.idLength;

    // Validate we have enough data
    if (data.size() < pixelOffset)
    {
        LX_ENGINE_ERROR("[Texture] TGA truncated at header");
        return false;
    }

    // Calculate expected pixel data size
    uint32_t bytesPerPixel     = header.bitDepth / 8;
    size_t   expectedPixelSize = 0;
    size_t   pixelCount        = 0;
    if (!CheckedMul(static_cast<size_t>(width), static_cast<size_t>(height), pixelCount) ||
        !CheckedMul(pixelCount, static_cast<size_t>(bytesPerPixel), expectedPixelSize))
    {
        LX_ENGINE_ERROR("[Texture] TGA dimensions overflow size calculation: {}x{}", width, height);
        return false;
    }

    if (data.size() < pixelOffset + expectedPixelSize)
    {
        // For RLE, we can't predict exact size, but need at least some data
        if (header.imageType == TGA_TYPE_RUN_LENGTH_ENCODED)
        {
            if (data.size() <= pixelOffset)
            {
                LX_ENGINE_ERROR("[Texture] TGA RLE: no pixel data");
                return false;
            }
        }
        else
        {
            LX_ENGINE_ERROR("[Texture] TGA truncated: expected {} pixel bytes, have {}", expectedPixelSize, data.size() - pixelOffset);
            return false;
        }
    }

    // Decode pixels
    std::vector<uint8_t> rgbaPixels;
    size_t               rgbaSize = 0;
    if (!CheckedMul(pixelCount, size_t(4), rgbaSize))
    {
        LX_ENGINE_ERROR("[Texture] TGA dimensions overflow RGBA output size: {}x{}", width, height);
        return false;
    }
    rgbaPixels.resize(rgbaSize); // Always output RGBA8

    if (header.imageType == TGA_TYPE_UNCOMPRESSED_RGB)
    {
        // Uncompressed: direct copy/padding
        const uint8_t* src = data.data() + pixelOffset;
        uint8_t*       dst = rgbaPixels.data();

        for (size_t i = 0; i < pixelCount; ++i)
        {
            // TGA stores as BGR or BGRA
            if (header.bitDepth == 24)
            {
                dst[0] = src[2]; // R
                dst[1] = src[1]; // G
                dst[2] = src[0]; // B
                dst[3] = 0xFF;   // A (pad to opaque)
                src += 3;
            }
            else // 32-bit
            {
                dst[0] = src[2]; // R
                dst[1] = src[1]; // G
                dst[2] = src[0]; // B
                dst[3] = src[3]; // A
                src += 4;
            }
            dst += 4;
        }
    }
    else // TGA_TYPE_RUN_LENGTH_ENCODED
    {
        // RLE decompression
        const uint8_t* src               = data.data() + pixelOffset;
        const uint8_t* srcEnd            = data.data() + data.size();
        uint8_t*       dst               = rgbaPixels.data();
        size_t         decodedPixelCount = 0;

        while (decodedPixelCount < pixelCount && src < srcEnd)
        {
            uint8_t packet = *src++;
            uint8_t count  = (packet & 0x7F) + 1; // Low 7 bits + 1

            if (packet & 0x80) // Run-length packet
            {
                // Pixel repeats 'count' times
                uint8_t r, g, b, a;

                if (header.bitDepth == 24)
                {
                    if (src + 3 > srcEnd)
                    {
                        LX_ENGINE_ERROR("[Texture] TGA RLE truncated in run packet");
                        return false;
                    }
                    b = src[0];
                    g = src[1];
                    r = src[2];
                    a = 0xFF;
                    src += 3;
                }
                else // 32-bit
                {
                    if (src + 4 > srcEnd)
                    {
                        LX_ENGINE_ERROR("[Texture] TGA RLE truncated in run packet");
                        return false;
                    }
                    b = src[0];
                    g = src[1];
                    r = src[2];
                    a = src[3];
                    src += 4;
                }

                for (uint8_t i = 0; i < count && decodedPixelCount < pixelCount; ++i)
                {
                    dst[0] = r;
                    dst[1] = g;
                    dst[2] = b;
                    dst[3] = a;
                    dst += 4;
                    decodedPixelCount++;
                }
            }
            else // Raw packet
            {
                // 'count' unique pixels follow
                for (uint8_t i = 0; i < count && decodedPixelCount < pixelCount; ++i)
                {
                    if (header.bitDepth == 24)
                    {
                        if (src + 3 > srcEnd)
                        {
                            LX_ENGINE_ERROR("[Texture] TGA RLE truncated in raw packet");
                            return false;
                        }
                        dst[0] = src[2]; // R
                        dst[1] = src[1]; // G
                        dst[2] = src[0]; // B
                        dst[3] = 0xFF;   // A
                        src += 3;
                    }
                    else // 32-bit
                    {
                        if (src + 4 > srcEnd)
                        {
                            LX_ENGINE_ERROR("[Texture] TGA RLE truncated in raw packet");
                            return false;
                        }
                        dst[0] = src[2]; // R
                        dst[1] = src[1]; // G
                        dst[2] = src[0]; // B
                        dst[3] = src[3]; // A
                        src += 4;
                    }
                    dst += 4;
                    decodedPixelCount++;
                }
            }
        }

        if (decodedPixelCount < pixelCount)
        {
            LX_ENGINE_ERROR("[Texture] TGA RLE underflow: decoded {} of {} pixels", decodedPixelCount, pixelCount);
            return false;
        }
    }

    // TGA origin bit:
    // 0 = bottom-left origin (needs flip for top-left render conventions)
    // 1 = top-left origin (already correct)
    if (!topOrigin)
    {
        uint32_t             rowBytes = width * 4;
        std::vector<uint8_t> tempRow(rowBytes);

        for (uint32_t y = 0; y < height / 2; ++y)
        {
            uint8_t* topRow    = rgbaPixels.data() + y * rowBytes;
            uint8_t* bottomRow = rgbaPixels.data() + (height - 1 - y) * rowBytes;

            std::memcpy(tempRow.data(), topRow, rowBytes);
            std::memcpy(topRow, bottomRow, rowBytes);
            std::memcpy(bottomRow, tempRow.data(), rowBytes);
        }
    }

    // Output
    out.Width  = width;
    out.Height = height;
    out.Format = TextureFormat::RGBA8;
    out.Pixels = std::move(rgbaPixels);

    return true;
}

// =============================================================================
// LoadMSK
// Loads MSK alpha mask files (8-bit grayscale)
// MSK format: Simple header followed by raw 8-bit grayscale data
// =============================================================================

bool TextureLoader::LoadMSK(const std::vector<uint8_t>& data,
                            uint32_t                   expectedWidth,
                            uint32_t                   expectedHeight,
                            std::vector<uint8_t>&      outAlphaData,
                            uint32_t&                  outWidth,
                            uint32_t&                  outHeight)
{
    outAlphaData.clear();
    outWidth  = 0;
    outHeight = 0;

    // MSK files have a simple header format
    // Based on the old client, the format is:
    // - 4 bytes: magic/signature
    // - 4 bytes: width
    // - 4 bytes: height
    // - Followed by width * height bytes of 8-bit grayscale alpha data
    if (data.size() >= 12)
    {
        uint32_t width  = *reinterpret_cast<const uint32_t*>(&data[4]);
        uint32_t height = *reinterpret_cast<const uint32_t*>(&data[8]);
        if (width > 0 && height > 0)
        {
            size_t expectedSize = 0;
            if (CheckedMul(static_cast<size_t>(width), static_cast<size_t>(height), expectedSize))
            {
                const bool matchesExpected = (expectedWidth == 0 || expectedHeight == 0) ||
                                             (width == expectedWidth && height == expectedHeight);
                if (matchesExpected && data.size() >= 12 + expectedSize)
                {
                    outAlphaData.assign(data.begin() + 12, data.begin() + 12 + expectedSize);
                    outWidth  = width;
                    outHeight = height;
                    LX_ENGINE_INFO("[Texture] Loaded MSK mask (headered): {}x{}", width, height);
                    return true;
                }
            }
        }
    }

    if (expectedWidth == 0 || expectedHeight == 0)
    {
        LX_ENGINE_WARN("[Texture] MSK decode failed: no header match and no expected size provided");
        return false;
    }

    size_t pixelCount = 0;
    if (!CheckedMul(static_cast<size_t>(expectedWidth), static_cast<size_t>(expectedHeight), pixelCount))
    {
        LX_ENGINE_ERROR("[Texture] MSK expected dimensions overflow size calculation: {}x{}", expectedWidth, expectedHeight);
        return false;
    }

    const size_t bitmaskSize = (pixelCount + 7u) / 8u;
    const size_t bitmaskSizeLegacy = (pixelCount / 8u) + 1u;
    if (data.size() == pixelCount)
    {
        outAlphaData.assign(data.begin(), data.end());
        outWidth  = expectedWidth;
        outHeight = expectedHeight;
        LX_ENGINE_INFO("[Texture] Loaded MSK mask (raw 8-bit): {}x{}", outWidth, outHeight);
        return true;
    }

    if (data.size() == bitmaskSize || data.size() == bitmaskSizeLegacy)
    {
        outAlphaData.resize(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const uint8_t byte = data[i / 8u];
            const uint8_t bit  = static_cast<uint8_t>((byte >> (i % 8u)) & 0x1u);
            outAlphaData[i]    = bit ? 0xFF : 0x00;
        }
        outWidth  = expectedWidth;
        outHeight = expectedHeight;
        LX_ENGINE_INFO("[Texture] Loaded MSK mask (bitmask): {}x{}", outWidth, outHeight);
        return true;
    }

    LX_ENGINE_WARN("[Texture] MSK decode failed: unsupported size {} for expected {}x{}", data.size(), expectedWidth, expectedHeight);
    return false;
}

// =============================================================================
// ApplyMSKMask
// Applies an MSK alpha mask to RGBA texture data
// =============================================================================

void TextureLoader::ApplyMSKMask(TextureData& textureData, const std::vector<uint8_t>& alphaMaskData)
{
    if (textureData.Format != TextureFormat::RGBA8)
    {
        LX_ENGINE_WARN("[Texture] Cannot apply MSK mask to non-RGBA8 texture");
        return;
    }

    const uint32_t pixelCount = textureData.Width * textureData.Height;
    if (alphaMaskData.size() != pixelCount)
    {
        LX_ENGINE_ERROR("[Texture] MSK mask dimension mismatch: texture is {}x{}, mask has {} pixels",
                        textureData.Width,
                        textureData.Height,
                        alphaMaskData.size());
        return;
    }

    // Replace alpha channel with mask data
    uint8_t* dst = textureData.Pixels.data();
    for (uint32_t i = 0; i < pixelCount; ++i)
    {
        dst[3] = alphaMaskData[i]; // Replace alpha with mask value
        dst += 4;
    }

    LX_ENGINE_INFO("[Texture] Applied MSK mask to texture ({}x{})", textureData.Width, textureData.Height);
}

} // namespace LXEngine
