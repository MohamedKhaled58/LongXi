#include "Map/DmapParser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "Core/FileSystem/PathUtils.h"
#include "Core/Logging/LogMacros.h"
#include "Map/MapBinaryReader.h"
#include "Map/TileGrid.h"

namespace LXMap
{

namespace
{

constexpr uint32_t kLegacyMapPathBytes = 260;

void AddWarning(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LX_MAP_WARN("[Map] {}", message);
}

std::string NormalizeResourcePath(const std::string& value)
{
    return LXCore::NormalizeVirtualResourcePath(value, true);
}

std::string BaseFileNameWithoutExtension(const std::string& path)
{
    if (path.empty())
    {
        return {};
    }

    size_t start = path.find_last_of('/');
    start        = (start == std::string::npos) ? 0 : start + 1;
    size_t end   = path.find_last_of('.');
    if (end == std::string::npos || end <= start)
    {
        end = path.size();
    }

    return path.substr(start, end - start);
}

} // namespace

bool ParseDmap(const std::string&          mapPath,
               const std::vector<uint8_t>& bytes,
               MapDescriptor&              outDescriptor,
               TileGrid&                   outTileGrid,
               size_t&                     outCursor,
               std::vector<std::string>&   outWarnings)
{
    constexpr size_t kHeaderSize = 8 + kLegacyMapPathBytes + 8;
    if (bytes.size() < kHeaderSize)
    {
        AddWarning(outWarnings, "DMAP header too small");
        return false;
    }

    const uint32_t version = ReadU32(bytes, 0);
    if (version != 1004)
    {
        AddWarning(outWarnings, "Unsupported dmap version " + std::to_string(version) + " (expected 1004)");
        return false;
    }

    const std::string puzzlePath = NormalizeResourcePath(ReadFixedCString(bytes, 8, kLegacyMapPathBytes));
    const uint32_t    width      = ReadU32(bytes, 268);
    const uint32_t    height     = ReadU32(bytes, 272);

    if (width == 0 || height == 0 || width > 4096 || height > 4096)
    {
        AddWarning(outWarnings, "Invalid map dimensions in dmap header");
        return false;
    }

    if (!outTileGrid.Initialize(width, height))
    {
        AddWarning(outWarnings, "Failed to initialize tile grid");
        return false;
    }

    outDescriptor.MapName         = BaseFileNameWithoutExtension(mapPath);
    outDescriptor.WidthInTiles    = width;
    outDescriptor.HeightInTiles   = height;
    outDescriptor.DmapVersion     = version;
    outDescriptor.PuzzleAssetPath = puzzlePath;
    outDescriptor.CellWidth       = 64;
    outDescriptor.CellHeight      = 32;
    outDescriptor.OriginX         = static_cast<int32_t>(outDescriptor.CellWidth * width / 2u);
    outDescriptor.OriginY         = static_cast<int32_t>(outDescriptor.CellHeight / 2u);

    size_t cursor = kHeaderSize;
    for (uint32_t y = 0; y < height; ++y)
    {
        uint32_t rowChecksum = 0;
        for (uint32_t x = 0; x < width; ++x)
        {
            if (cursor + 6 > bytes.size())
            {
                AddWarning(outWarnings, "DMAP tile payload truncated while reading terrain rows");
                return false;
            }

            const uint16_t mask     = ReadU16(bytes, cursor);
            const uint16_t terrain  = ReadU16(bytes, cursor + 2);
            const int16_t  altitude = ReadI16(bytes, cursor + 4);
            cursor += 6;

            TileRecord tile;
            tile.TileX     = static_cast<int32_t>(x);
            tile.TileY     = static_cast<int32_t>(y);
            tile.Height    = altitude;
            tile.TextureId = kInvalidPuzzleIndex;
            tile.MaskId    = mask;
            tile.TerrainId = terrain;
            tile.Flags     = mask != 0 ? 1u : 0u;
            if (!outTileGrid.SetTile(tile))
            {
                AddWarning(outWarnings, "Tile grid write out of bounds while parsing dmap");
                return false;
            }

            const int32_t left  = static_cast<int32_t>(mask) * (static_cast<int32_t>(terrain) + static_cast<int32_t>(y) + 1);
            const int32_t right = (static_cast<int32_t>(altitude) + 2) * (static_cast<int32_t>(x) + 1 + static_cast<int32_t>(terrain));
            rowChecksum += static_cast<uint32_t>(left + right);
        }

        if (cursor + sizeof(uint32_t) > bytes.size())
        {
            AddWarning(outWarnings, "DMAP row checksum payload truncated");
            return false;
        }

        const uint32_t expectedChecksum = ReadU32(bytes, cursor);
        cursor += sizeof(uint32_t);
        if (rowChecksum != expectedChecksum)
        {
            AddWarning(outWarnings, "DMAP row checksum mismatch at row " + std::to_string(y));
            return false;
        }
    }

    outDescriptor.Passages.clear();
    outDescriptor.InteractiveObjectCount = 0;
    outDescriptor.SceneObjectCount       = 0;
    outDescriptor.UnknownObjectCount     = 0;
    outDescriptor.LoadedSceneFileCount   = 0;
    outDescriptor.LoadedScenePartCount   = 0;

    if (!ParsePassageBlock(bytes, cursor, outDescriptor.Passages, outWarnings))
    {
        return false;
    }

    outCursor = cursor;
    return true;
}

bool ParsePassageBlock(const std::vector<uint8_t>&    bytes,
                       size_t&                        ioCursor,
                       std::vector<MapPassageRecord>& outPassages,
                       std::vector<std::string>&      outWarnings)
{
    if (ioCursor + sizeof(uint32_t) > bytes.size())
    {
        AddWarning(outWarnings, "DMAP payload missing passage amount");
        return false;
    }

    const uint32_t passageCount = ReadU32(bytes, ioCursor);
    ioCursor += sizeof(uint32_t);
    outPassages.clear();
    outPassages.reserve(passageCount);

    for (uint32_t passageIndex = 0; passageIndex < passageCount; ++passageIndex)
    {
        if (ioCursor + sizeof(int32_t) * 3 > bytes.size())
        {
            AddWarning(outWarnings, "DMAP payload truncated while reading passage records");
            return false;
        }

        MapPassageRecord passage;
        passage.TileX            = ReadI32(bytes, ioCursor + 0);
        passage.TileY            = ReadI32(bytes, ioCursor + 4);
        passage.DestinationMapId = ReadI32(bytes, ioCursor + 8);
        ioCursor += sizeof(int32_t) * 3;
        outPassages.push_back(passage);
    }

    return true;
}

} // namespace LXMap
