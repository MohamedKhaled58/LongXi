#include "Map/AniParser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <string_view>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Map/MapBinaryReader.h"
#include "Map/MapTypes.h"

namespace LXMap
{

namespace
{

void AddWarning(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LX_MAP_WARN("[Map] {}", message);
}

std::string Trim(std::string value)
{
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return {};
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool LooksLikeNumericId(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    return std::all_of(value.begin(),
                       value.end(),
                       [](unsigned char ch)
                       {
                           return std::isdigit(ch) != 0;
                       });
}

// cellLayerStride: bytes per cell in the layer data.
// Old client (2009): uint16 terrain + uint16 mask + int16 altitude = 6 bytes.
// Some data versions use int32 terrain + int32 mask + int32 altitude = 12 bytes.
bool TryParseTerrainPartRecord(const std::vector<uint8_t>& bytes,
                               size_t&                     ioCursor,
                               TerrainPartDescriptor&      outDescriptor,
                               size_t                      cellLayerStride)
{
    constexpr size_t kAniPathBytes  = 256;
    constexpr size_t kAniTitleBytes = 64;
    constexpr size_t kHeaderBytes   = kAniPathBytes + kAniTitleBytes + sizeof(int32_t) * 9;

    if (ioCursor + kHeaderBytes > bytes.size())
    {
        return false;
    }

    const size_t base           = ioCursor;
    outDescriptor.AniPath       = LXCore::NormalizeVirtualResourcePath(ReadFixedCString(bytes, base + 0, kAniPathBytes), true);
    outDescriptor.AniTag        = ReadFixedCString(bytes, base + kAniPathBytes, kAniTitleBytes);
    outDescriptor.SpriteOffsetX = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 0);
    outDescriptor.SpriteOffsetY = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 4);
    outDescriptor.FrameInterval = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 8);
    const uint32_t baseWidth    = ReadU32(bytes, base + kAniPathBytes + kAniTitleBytes + 12);
    const uint32_t baseHeight   = ReadU32(bytes, base + kAniPathBytes + kAniTitleBytes + 16);
    outDescriptor.BaseWidth     = static_cast<int32_t>(baseWidth);
    outDescriptor.BaseHeight    = static_cast<int32_t>(baseHeight);
    outDescriptor.ShowType      = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 20);
    outDescriptor.SceneOffsetX  = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 24);
    outDescriptor.SceneOffsetY  = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 28);
    outDescriptor.HeightOffset  = ReadI32(bytes, base + kAniPathBytes + kAniTitleBytes + 32);

    if (baseWidth == 0 || baseHeight == 0 || baseWidth > 512 || baseHeight > 512)
    {
        return false;
    }

    const uint64_t cellCount = static_cast<uint64_t>(baseWidth) * static_cast<uint64_t>(baseHeight);
    if (cellLayerStride == 0 || cellCount > (std::numeric_limits<size_t>::max)() / cellLayerStride)
    {
        return false;
    }

    const size_t layerBytes = static_cast<size_t>(cellCount) * cellLayerStride;
    if (base + kHeaderBytes + layerBytes > bytes.size())
    {
        return false;
    }

    ioCursor = base + kHeaderBytes + layerBytes;
    return true;
}

} // namespace

bool ParseAni(const std::string&                                      aniPath,
              CVirtualFileSystem&                                     vfs,
              std::unordered_map<uint16_t, std::vector<std::string>>& outFramesByPuzzle,
              std::vector<std::string>&                               outWarnings)
{
    const std::vector<uint8_t> bytes = vfs.ReadAll(aniPath);
    if (bytes.empty())
    {
        AddWarning(outWarnings, "ANI file missing or empty: " + aniPath);
        return false;
    }

    std::string text(bytes.begin(), bytes.end());
    std::replace(text.begin(), text.end(), '\r', '\n');

    uint16_t           currentPuzzleIndex = kInvalidPuzzleIndex;
    std::istringstream stream(text);
    std::string        line;
    while (std::getline(stream, line))
    {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
        {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']')
        {
            currentPuzzleIndex       = kInvalidPuzzleIndex;
            std::string section      = trimmed.substr(1, trimmed.size() - 2);
            section                  = Trim(section);
            std::string sectionLower = section;
            std::transform(sectionLower.begin(),
                           sectionLower.end(),
                           sectionLower.begin(),
                           [](unsigned char ch)
                           {
                               return static_cast<char>(std::tolower(ch));
                           });

            constexpr char kPuzzlePrefix[] = "puzzle";
            if (sectionLower.rfind(kPuzzlePrefix, 0) == 0)
            {
                const std::string idPart = sectionLower.substr(std::strlen(kPuzzlePrefix));
                if (LooksLikeNumericId(idPart))
                {
                    try
                    {
                        const uint32_t id = static_cast<uint32_t>(std::stoul(idPart));
                        if (id <= std::numeric_limits<uint16_t>::max())
                        {
                            currentPuzzleIndex = static_cast<uint16_t>(id);
                        }
                    }
                    catch (const std::exception&)
                    {
                        currentPuzzleIndex = kInvalidPuzzleIndex;
                    }
                }
            }

            continue;
        }

        if (currentPuzzleIndex == kInvalidPuzzleIndex)
        {
            continue;
        }

        const size_t equalsPos = trimmed.find('=');
        if (equalsPos == std::string::npos)
        {
            continue;
        }

        std::string key   = Trim(trimmed.substr(0, equalsPos));
        std::string value = Trim(trimmed.substr(equalsPos + 1));
        if (value.empty())
        {
            continue;
        }

        std::transform(key.begin(),
                       key.end(),
                       key.begin(),
                       [](unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });

        if (key == "frameamount")
        {
            continue;
        }

        if (key.rfind("frame", 0) == 0)
        {
            outFramesByPuzzle[currentPuzzleIndex].push_back(value);
        }
    }

    return true;
}

bool ParseTerrainPartDescriptors(const std::vector<uint8_t>& bytes, std::vector<TerrainPartDescriptor>& outDescriptors)
{
    outDescriptors.clear();
    if (bytes.empty())
    {
        return false;
    }

    // Try multiple cell-layer strides: 6 bytes (uint16+uint16+int16) is the old 2009 format,
    // 12 bytes (uint32+int32+int32) is an alternate format found in some data files.
    constexpr size_t kCellStrides[] = {6, 12};
    constexpr size_t kStrideCount   = sizeof(kCellStrides) / sizeof(kCellStrides[0]);

    auto tryParseWithCountHeader = [&bytes](size_t cellStride, std::vector<TerrainPartDescriptor>& outParsed) -> bool
    {
        if (bytes.size() < sizeof(uint32_t))
        {
            return false;
        }

        const uint32_t partCount = ReadU32(bytes, 0);
        if (partCount == 0 || partCount > 4096)
        {
            return false;
        }

        size_t                             cursor = sizeof(uint32_t);
        std::vector<TerrainPartDescriptor> parsedDescriptors;
        parsedDescriptors.reserve(partCount);
        for (uint32_t partIndex = 0; partIndex < partCount; ++partIndex)
        {
            TerrainPartDescriptor descriptor;
            if (!TryParseTerrainPartRecord(bytes, cursor, descriptor, cellStride))
            {
                return false;
            }

            parsedDescriptors.push_back(std::move(descriptor));
        }

        outParsed = std::move(parsedDescriptors);
        return true;
    };

    // Strategy 1: Try count-header format with each stride.
    for (size_t strideIndex = 0; strideIndex < kStrideCount; ++strideIndex)
    {
        std::vector<TerrainPartDescriptor> parsed;
        if (tryParseWithCountHeader(kCellStrides[strideIndex], parsed))
        {
            outDescriptors = std::move(parsed);
            return true;
        }
    }

    // Strategy 2: Try sequential records without a count header, with each stride.
    for (size_t strideIndex = 0; strideIndex < kStrideCount; ++strideIndex)
    {
        std::vector<TerrainPartDescriptor> parsed;
        size_t                             cursor = 0;
        bool                               failed = false;
        while (cursor < bytes.size())
        {
            TerrainPartDescriptor descriptor;
            const size_t          recordStart = cursor;
            if (!TryParseTerrainPartRecord(bytes, cursor, descriptor, kCellStrides[strideIndex]))
            {
                if (recordStart == 0)
                {
                    failed = true;
                }
                break;
            }

            parsed.push_back(std::move(descriptor));
        }

        if (!failed && !parsed.empty())
        {
            outDescriptors = std::move(parsed);
            return true;
        }
    }

    return false;
}

} // namespace LXMap
