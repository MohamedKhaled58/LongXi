#include "Map/MapLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Map/TileGrid.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LongXi
{

namespace
{
constexpr uint32_t kLegacyMapPathBytes  = 260;
constexpr uint32_t kLegacyMapTitleBytes = 128;

constexpr int32_t kLegacyMapObjTerrain     = 1;
constexpr int32_t kLegacyMapObjScene       = 3;
constexpr int32_t kLegacyMapObjCover       = 4;
constexpr int32_t kLegacyMapObjPuzzle      = 8;
constexpr int32_t kLegacyMapObj3DEffect    = 10;
constexpr int32_t kLegacyMapObjSound       = 15;
constexpr int32_t kLegacyMapObj3DEffectNew = 19;

constexpr int32_t kLegacyLayerScene = 4;

bool IsLikelyInteractiveObjectType(int32_t objectType)
{
    switch (objectType)
    {
        case kLegacyMapObjTerrain:
        case kLegacyMapObjScene:
        case kLegacyMapObjCover:
        case kLegacyMapObjPuzzle:
        case kLegacyMapObj3DEffect:
        case kLegacyMapObjSound:
        case kLegacyMapObj3DEffectNew:
            return true;
        default:
            return false;
    }
}

float ReadF32(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + sizeof(float) > data.size())
    {
        return 0.0f;
    }

    float value = 0.0f;
    memcpy(&value, data.data() + offset, sizeof(float));
    return value;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool EndsWithInsensitive(const std::string& value, std::string_view suffix)
{
    if (value.size() < suffix.size())
    {
        return false;
    }

    const size_t start = value.size() - suffix.size();
    for (size_t index = 0; index < suffix.size(); ++index)
    {
        const unsigned char left  = static_cast<unsigned char>(value[start + index]);
        const unsigned char right = static_cast<unsigned char>(suffix[index]);
        if (std::tolower(left) != std::tolower(right))
        {
            return false;
        }
    }

    return true;
}

bool IsSupportedTexturePath(const std::string& path)
{
    return EndsWithInsensitive(path, ".dds") || EndsWithInsensitive(path, ".tga");
}

bool IsVirtualRootedPath(const std::string& path)
{
    const std::string normalizedPath = ToLowerAscii(NormalizeVirtualResourcePath(path, true));
    if (normalizedPath.empty())
    {
        return false;
    }

    if (normalizedPath.find(':') != std::string::npos || normalizedPath.front() == '/')
    {
        return true;
    }

    return normalizedPath.rfind("data/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("ani/", 0) == 0;
}

bool LooksLikeTexturePayload(const std::string& normalizedPath, const std::vector<uint8_t>& bytes)
{
    if (EndsWithInsensitive(normalizedPath, ".dds"))
    {
        return bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ';
    }

    if (EndsWithInsensitive(normalizedPath, ".tga"))
    {
        if (bytes.size() < 18)
        {
            return false;
        }

        const uint8_t imageType = bytes[2];
        switch (imageType)
        {
            case 1:
            case 2:
            case 3:
            case 9:
            case 10:
            case 11:
                return true;
            default:
                return false;
        }
    }

    return false;
}

struct TerrainPartDescriptor
{
    std::string AniPath;
    std::string AniTag;
    int32_t     SpriteOffsetX = 0;
    int32_t     SpriteOffsetY = 0;
    int32_t     SceneOffsetX  = 0;
    int32_t     SceneOffsetY  = 0;
    int32_t     HeightOffset  = 0;
};

bool TryParseTerrainPartRecord(const std::vector<uint8_t>& bytes, size_t& ioCursor, TerrainPartDescriptor& outDescriptor)
{
    constexpr size_t kAniPathBytes  = 256;
    constexpr size_t kAniTitleBytes = 64;
    constexpr size_t kHeaderBytes   = kAniPathBytes + kAniTitleBytes + sizeof(int32_t) * 9;

    if (ioCursor + kHeaderBytes > bytes.size())
    {
        return false;
    }

    auto readU32 = [&bytes](size_t offset) -> uint32_t
    {
        if (offset + sizeof(uint32_t) > bytes.size())
        {
            return 0;
        }

        return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    };

    auto readI32 = [&readU32](size_t offset) -> int32_t
    {
        return static_cast<int32_t>(readU32(offset));
    };

    auto readCString = [&bytes](size_t offset, size_t length) -> std::string
    {
        if (offset >= bytes.size() || length == 0)
        {
            return {};
        }

        const size_t end       = std::min(bytes.size(), offset + length);
        size_t       actualEnd = offset;
        while (actualEnd < end && bytes[actualEnd] != 0)
        {
            ++actualEnd;
        }

        return std::string(reinterpret_cast<const char*>(bytes.data() + offset), actualEnd - offset);
    };

    const size_t base           = ioCursor;
    outDescriptor.AniPath       = NormalizeVirtualResourcePath(readCString(base + 0, kAniPathBytes), true);
    outDescriptor.AniTag        = readCString(base + kAniPathBytes, kAniTitleBytes);
    outDescriptor.SpriteOffsetX = readI32(base + kAniPathBytes + kAniTitleBytes + 0);
    outDescriptor.SpriteOffsetY = readI32(base + kAniPathBytes + kAniTitleBytes + 4);
    const uint32_t baseWidth    = readU32(base + kAniPathBytes + kAniTitleBytes + 12);
    const uint32_t baseHeight   = readU32(base + kAniPathBytes + kAniTitleBytes + 16);
    outDescriptor.SceneOffsetX  = readI32(base + kAniPathBytes + kAniTitleBytes + 24);
    outDescriptor.SceneOffsetY  = readI32(base + kAniPathBytes + kAniTitleBytes + 28);
    outDescriptor.HeightOffset  = readI32(base + kAniPathBytes + kAniTitleBytes + 32);

    if (baseWidth == 0 || baseHeight == 0 || baseWidth > 512 || baseHeight > 512)
    {
        return false;
    }

    const uint64_t cellCount = static_cast<uint64_t>(baseWidth) * static_cast<uint64_t>(baseHeight);
    if (cellCount > (std::numeric_limits<size_t>::max)() / (sizeof(uint32_t) + sizeof(int32_t) * 2))
    {
        return false;
    }

    const size_t layerBytes = static_cast<size_t>(cellCount) * (sizeof(uint32_t) + sizeof(int32_t) * 2);
    if (base + kHeaderBytes + layerBytes > bytes.size())
    {
        return false;
    }

    ioCursor = base + kHeaderBytes + layerBytes;
    return true;
}

bool ParseTerrainPartDescriptors(const std::vector<uint8_t>& bytes, std::vector<TerrainPartDescriptor>& outDescriptors)
{
    outDescriptors.clear();
    if (bytes.empty())
    {
        return false;
    }

    auto tryParseWithCountHeader = [&bytes, &outDescriptors]() -> bool
    {
        if (bytes.size() < sizeof(uint32_t))
        {
            return false;
        }

        const uint32_t partCount = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
                                   (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
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
            if (!TryParseTerrainPartRecord(bytes, cursor, descriptor))
            {
                return false;
            }

            parsedDescriptors.push_back(std::move(descriptor));
        }

        outDescriptors = std::move(parsedDescriptors);
        return true;
    };

    if (tryParseWithCountHeader())
    {
        return true;
    }

    size_t cursor = 0;
    while (cursor < bytes.size())
    {
        TerrainPartDescriptor descriptor;
        const size_t          recordStart = cursor;
        if (!TryParseTerrainPartRecord(bytes, cursor, descriptor))
        {
            if (recordStart == 0)
            {
                outDescriptors.clear();
                return false;
            }
            break;
        }

        outDescriptors.push_back(std::move(descriptor));
    }

    return !outDescriptors.empty();
}

std::string FileNameFromPath(const std::string& path)
{
    if (path.empty())
    {
        return {};
    }

    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
    {
        return path;
    }

    return path.substr(slash + 1);
}

std::string FileStem(const std::string& path)
{
    const std::string fileName = FileNameFromPath(path);
    if (fileName.empty())
    {
        return {};
    }

    const size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
    {
        return fileName;
    }

    return fileName.substr(0, dot);
}

void ExtractEmbeddedPathHints(const std::vector<uint8_t>& payload, std::vector<std::string>& outHints)
{
    outHints.clear();
    std::string token;
    token.reserve(256);

    auto flushToken = [&token, &outHints]()
    {
        if (token.empty())
        {
            return;
        }

        std::string normalized = NormalizeVirtualResourcePath(token, true);
        token.clear();
        if (normalized.empty())
        {
            return;
        }

        if (!EndsWithInsensitive(normalized, ".part") && !EndsWithInsensitive(normalized, ".scene"))
        {
            return;
        }

        if (std::ranges::find(outHints, normalized) == outHints.end())
        {
            outHints.push_back(std::move(normalized));
        }
    };

    for (uint8_t byteValue : payload)
    {
        const char character  = static_cast<char>(byteValue);
        const bool isPathChar = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                                (character >= '0' && character <= '9') || character == '.' || character == '_' || character == '-' ||
                                character == '/' || character == '\\';
        if (isPathChar)
        {
            token.push_back(character);
        }
        else
        {
            flushToken();
        }
    }

    flushToken();
}

} // namespace

bool MapLoader::LoadMap(const std::string&                                      mapPath,
                        CVirtualFileSystem&                                     vfs,
                        TextureManager&                                         textureManager,
                        MapDescriptor&                                          outDescriptor,
                        TileGrid&                                               outTileGrid,
                        std::vector<MapObjectRecord>&                           outMapObjects,
                        std::vector<MapAnimationState>&                         outAnimations,
                        std::unordered_map<uint16_t, std::shared_ptr<Texture>>& outTextureRefs,
                        std::vector<std::string>&                               outWarnings)
{
    outDescriptor = {};
    outTileGrid.Clear();
    outMapObjects.clear();
    outAnimations.clear();
    outTextureRefs.clear();
    outWarnings.clear();

    std::string resolvedMapPath;
    uint32_t    resolvedMapId    = 0;
    uint32_t    resolvedMapFlags = 0;
    if (!ResolveMapPath(mapPath, vfs, resolvedMapPath, resolvedMapId, resolvedMapFlags, outWarnings))
    {
        return false;
    }

    outDescriptor.MapId        = resolvedMapId;
    outDescriptor.GameMapFlags = resolvedMapFlags;
    outDescriptor.SourcePath   = resolvedMapPath;

    if (!ParseDmap(resolvedMapPath, vfs, outDescriptor, outTileGrid, outMapObjects, outWarnings))
    {
        return false;
    }

    if (!ParsePuzzle(resolvedMapPath, vfs, textureManager, outDescriptor, outTileGrid, outAnimations, outTextureRefs, outWarnings))
    {
        AddWarning(outWarnings, "Puzzle parsing reported invalid data; continuing with fallback visuals");
    }

    return outDescriptor.IsValid() && outTileGrid.IsValid();
}

bool MapLoader::ParseDmap(const std::string&            mapPath,
                          CVirtualFileSystem&           vfs,
                          MapDescriptor&                outDescriptor,
                          TileGrid&                     outTileGrid,
                          std::vector<MapObjectRecord>& outMapObjects,
                          std::vector<std::string>&     outWarnings)
{
    const std::vector<uint8_t> bytes = vfs.ReadAll(mapPath);
    if (bytes.empty())
    {
        AddWarning(outWarnings, "Map file not found or empty: " + mapPath);
        return false;
    }

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

    uint32_t nextObjectId = 1;
    if (!ParsePassageBlock(bytes, cursor, outDescriptor.Passages, outWarnings))
    {
        return false;
    }

    uint32_t parsedInteractiveObjects  = 0;
    uint32_t unknownInteractiveObjects = 0;
    bool     interactiveParsingAborted = false;
    if (!ParseInteractiveObjectBlock(bytes,
                                     cursor,
                                     outDescriptor,
                                     outMapObjects,
                                     nextObjectId,
                                     parsedInteractiveObjects,
                                     unknownInteractiveObjects,
                                     interactiveParsingAborted,
                                     outWarnings))
    {
        return false;
    }

    uint32_t parsedSceneObjects  = 0;
    uint32_t unknownSceneObjects = 0;
    if (interactiveParsingAborted)
    {
        AddWarning(outWarnings, "Interactive object parsing was incomplete; attempting scene-layer parse from current position");

        auto looksLikeSceneLayerBlockAt = [&bytes](size_t offset) -> bool
        {
            if (offset + sizeof(uint32_t) > bytes.size())
            {
                return false;
            }

            const uint32_t layerCount = ReadU32(bytes, offset);
            if (layerCount > 2048)
            {
                return false;
            }

            if (layerCount == 0)
            {
                return true;
            }

            if (offset + sizeof(uint32_t) + sizeof(int32_t) * 2 > bytes.size())
            {
                return false;
            }

            const int32_t firstLayerType = ReadI32(bytes, offset + sizeof(uint32_t) + sizeof(int32_t));
            if (firstLayerType != kLegacyLayerScene)
            {
                return false;
            }

            if (offset + sizeof(uint32_t) + sizeof(int32_t) * 5 > bytes.size())
            {
                return false;
            }

            const int32_t firstSceneObjectCount = ReadI32(bytes, offset + sizeof(uint32_t) + sizeof(int32_t) * 4);
            return firstSceneObjectCount >= 0 && firstSceneObjectCount <= 100000;
        };

        if (!looksLikeSceneLayerBlockAt(cursor))
        {
            constexpr size_t kSceneResyncMaxScanBytes = 1024 * 1024;
            const size_t     scanEnd                  = std::min(bytes.size(), cursor + kSceneResyncMaxScanBytes);
            size_t           recoveredCursor          = cursor;
            bool             foundSceneCursor         = false;
            for (size_t candidate = cursor + sizeof(uint32_t); candidate + sizeof(uint32_t) <= scanEnd; candidate += sizeof(uint32_t))
            {
                if (!looksLikeSceneLayerBlockAt(candidate))
                {
                    continue;
                }

                recoveredCursor  = candidate;
                foundSceneCursor = true;
                break;
            }

            if (foundSceneCursor && recoveredCursor > cursor)
            {
                AddWarning(outWarnings,
                           "Recovered scene-layer parse cursor by skipping " + std::to_string(recoveredCursor - cursor) +
                               " bytes after interactive parse failure");
                cursor = recoveredCursor;
            }
            else
            {
                AddWarning(outWarnings, "Could not recover scene-layer parse cursor after interactive parse failure");
            }
        }
    }

    {
        bool sceneParsingAborted = false;
        if (!ParseSceneLayerBlock(bytes,
                                  cursor,
                                  outDescriptor,
                                  outMapObjects,
                                  nextObjectId,
                                  parsedSceneObjects,
                                  unknownSceneObjects,
                                  sceneParsingAborted,
                                  outWarnings))
        {
            AddWarning(outWarnings, "Scene-layer parsing failed; continuing with partial map data");
        }

        if (sceneParsingAborted)
        {
            AddWarning(outWarnings, "Scene-layer payload parsing stopped on unsupported legacy data");
        }
    }

    outDescriptor.InteractiveObjectCount = parsedInteractiveObjects;
    outDescriptor.SceneObjectCount       = parsedSceneObjects;
    outDescriptor.UnknownObjectCount     = unknownInteractiveObjects + unknownSceneObjects;

    for (const MapObjectRecord& objectRecord : outMapObjects)
    {
        TileRecord* tileRecord = outTileGrid.GetTile(objectRecord.TileX, objectRecord.TileY);
        if (tileRecord == nullptr)
        {
            continue;
        }

        tileRecord->ObjectRefs.push_back(objectRecord.ObjectId);
    }

    std::unordered_set<std::string> loadedSceneFiles;
    std::unordered_set<std::string> loadedScenePartFiles;
    uint32_t                        unresolvedAniTextureCount = 0;
    std::vector<std::string>        unresolvedAniSamples;
    std::vector<std::string>        pathHints;

    using AniSectionFrames = std::unordered_map<std::string, std::vector<std::string>>;
    std::unordered_map<std::string, AniSectionFrames> aniSectionsByPath;

    auto resolveMapObjectPath = [&vfs](const std::string& rawPath, std::string& outResolvedPath) -> bool
    {
        outResolvedPath.clear();

        const std::string normalizedRaw = NormalizeVirtualResourcePath(rawPath, true);
        if (normalizedRaw.empty())
        {
            return false;
        }

        auto tryPath = [&outResolvedPath, &vfs](const std::string& rawCandidate) -> bool
        {
            const std::string normalizedCandidate = NormalizeVirtualResourcePath(rawCandidate, true);
            if (normalizedCandidate.empty())
            {
                return false;
            }

            if (!vfs.Exists(normalizedCandidate))
            {
                return false;
            }

            outResolvedPath = normalizedCandidate;
            return true;
        };

        const std::string loweredRaw = ToLowerAscii(normalizedRaw);
        if (loweredRaw.rfind("ani/", 0) == 0)
        {
            return tryPath(normalizedRaw);
        }

        if (loweredRaw.rfind("data/ani/", 0) == 0)
        {
            return tryPath("ani/" + normalizedRaw.substr(9));
        }

        if (loweredRaw.rfind("map/", 0) == 0)
        {
            if (tryPath(normalizedRaw))
            {
                return true;
            }
            return tryPath("data/" + normalizedRaw);
        }

        if (loweredRaw.rfind("data/map/", 0) == 0)
        {
            if (tryPath(normalizedRaw))
            {
                return true;
            }
            return tryPath(normalizedRaw.substr(5));
        }

        const std::string fileName  = FileNameFromPath(normalizedRaw);
        const size_t      dotPos    = fileName.find_last_of('.');
        const std::string extension = (dotPos == std::string::npos) ? std::string() : ToLowerAscii(fileName.substr(dotPos));

        if (extension == ".ani")
        {
            if (tryPath("ani/" + fileName))
            {
                return true;
            }

            return tryPath(normalizedRaw);
        }

        if (extension == ".scene")
        {
            if (tryPath("map/scene/" + fileName))
            {
                return true;
            }
            if (tryPath("data/map/scene/" + fileName))
            {
                return true;
            }
        }
        else if (extension == ".part")
        {
            if (tryPath("map/scenepart/" + fileName))
            {
                return true;
            }
            if (tryPath("data/map/scenepart/" + fileName))
            {
                return true;
            }
        }

        if (tryPath("map/" + normalizedRaw))
        {
            return true;
        }

        return tryPath("data/map/" + normalizedRaw);
    };

    auto loadAniSections = [&vfs, &aniSectionsByPath](const std::string& aniPath) -> const AniSectionFrames*
    {
        const auto cacheIt = aniSectionsByPath.find(aniPath);
        if (cacheIt != aniSectionsByPath.end())
        {
            return &cacheIt->second;
        }

        AniSectionFrames           sections;
        const std::vector<uint8_t> aniBytes = vfs.ReadAll(aniPath);
        if (!aniBytes.empty())
        {
            std::string text(aniBytes.begin(), aniBytes.end());
            std::replace(text.begin(), text.end(), '\r', '\n');

            std::istringstream stream(text);
            std::string        line;
            std::string        currentSection;
            while (std::getline(stream, line))
            {
                const std::string trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
                {
                    continue;
                }

                if (trimmed.front() == '[' && trimmed.back() == ']')
                {
                    currentSection = ToLowerAscii(Trim(trimmed.substr(1, trimmed.size() - 2)));
                    continue;
                }

                if (currentSection.empty())
                {
                    continue;
                }

                const size_t equalsPos = trimmed.find('=');
                if (equalsPos == std::string::npos)
                {
                    continue;
                }

                std::string       key   = Trim(trimmed.substr(0, equalsPos));
                const std::string value = Trim(trimmed.substr(equalsPos + 1));
                if (value.empty())
                {
                    continue;
                }

                key = ToLowerAscii(key);
                if (key == "frameamount")
                {
                    continue;
                }

                if (key.rfind("frame", 0) == 0)
                {
                    sections[currentSection].push_back(value);
                }
            }
        }

        const auto [insertedIt, _] = aniSectionsByPath.emplace(aniPath, std::move(sections));
        return &insertedIt->second;
    };

    auto resolveAniTexturePath =
        [&loadAniSections, &resolveMapObjectPath, &outDescriptor, &vfs](
            const std::string& aniPath, const std::string& resourceTag, std::string& outResolvedTexturePath) -> bool
    {
        outResolvedTexturePath.clear();

        const AniSectionFrames* sections = loadAniSections(aniPath);
        if (sections == nullptr || sections->empty())
        {
            return false;
        }

        std::vector<std::string> sectionCandidates;
        auto                     pushSectionCandidate = [&sectionCandidates](const std::string& rawValue)
        {
            std::string normalized = ToLowerAscii(Trim(rawValue));
            if (normalized.empty())
            {
                return;
            }

            if (std::ranges::find(sectionCandidates, normalized) == sectionCandidates.end())
            {
                sectionCandidates.push_back(normalized);
            }

            const std::string stem = FileStem(normalized);
            if (!stem.empty() && stem != normalized && std::ranges::find(sectionCandidates, stem) == sectionCandidates.end())
            {
                sectionCandidates.push_back(stem);
            }
        };

        pushSectionCandidate(resourceTag);

        const size_t colonPos = resourceTag.find_last_of(':');
        if (colonPos != std::string::npos && colonPos + 1 < resourceTag.size())
        {
            pushSectionCandidate(resourceTag.substr(colonPos + 1));
        }

        for (const std::string& sectionName : sectionCandidates)
        {
            const auto sectionIt = sections->find(sectionName);
            if (sectionIt == sections->end())
            {
                continue;
            }

            for (const std::string& framePath : sectionIt->second)
            {
                std::string resolvedFramePath;
                if (TryResolveTexturePath(framePath, outDescriptor, vfs, resolvedFramePath))
                {
                    outResolvedTexturePath = resolvedFramePath;
                    return true;
                }

                if (resolveMapObjectPath(framePath, resolvedFramePath) && IsSupportedTexturePath(resolvedFramePath))
                {
                    outResolvedTexturePath = resolvedFramePath;
                    return true;
                }
            }
        }

        return false;
    };

    auto registerScenePart = [&vfs, &loadedScenePartFiles, &outDescriptor, &outWarnings](const std::string& partPath)
    {
        if (!loadedScenePartFiles.insert(partPath).second)
        {
            return;
        }

        const std::vector<uint8_t> partBytes = vfs.ReadAll(partPath);
        if (partBytes.empty())
        {
            AddWarning(outWarnings, "Failed to load map ScenePart resource: " + partPath);
            return;
        }

        ++outDescriptor.LoadedScenePartCount;
    };

    auto appendUnresolvedAniSample =
        [&unresolvedAniTextureCount, &unresolvedAniSamples](const std::string& aniPath, const std::string& aniTag)
    {
        ++unresolvedAniTextureCount;
        if (unresolvedAniSamples.size() < 8)
        {
            unresolvedAniSamples.push_back(aniPath + ":" + aniTag);
        }
    };

    std::unordered_map<std::string, std::vector<TerrainPartDescriptor>> terrainPartDescriptorsByPath;
    std::vector<MapObjectRecord>                                        generatedMapObjects;
    generatedMapObjects.reserve(outMapObjects.size());

    auto resolveAniObjectToTexture =
        [&resolveMapObjectPath, &resolveAniTexturePath, &appendUnresolvedAniSample](MapObjectRecord& objectRecord) -> void
    {
        std::string resolvedObjectPath;
        if (resolveMapObjectPath(objectRecord.ResourcePath, resolvedObjectPath))
        {
            objectRecord.ResourcePath = resolvedObjectPath;
        }

        if (objectRecord.ResourcePath.empty() || !EndsWithInsensitive(objectRecord.ResourcePath, ".ani"))
        {
            return;
        }

        std::string resolvedTexturePath;
        if (resolveAniTexturePath(objectRecord.ResourcePath, objectRecord.ResourceTag, resolvedTexturePath))
        {
            objectRecord.ResourcePath = resolvedTexturePath;
        }
        else
        {
            appendUnresolvedAniSample(objectRecord.ResourcePath, objectRecord.ResourceTag);
            objectRecord.ResourcePath.clear();
        }
    };

    auto appendTerrainCompositeParts = [&nextObjectId,
                                        &terrainPartDescriptorsByPath,
                                        &resolveMapObjectPath,
                                        &resolveAniObjectToTexture,
                                        &generatedMapObjects,
                                        &outWarnings](const MapObjectRecord&      sourceObject,
                                                      const std::string&          compositePath,
                                                      const std::vector<uint8_t>& compositeBytes) -> void
    {
        const auto cacheIt = terrainPartDescriptorsByPath.find(compositePath);
        if (cacheIt == terrainPartDescriptorsByPath.end())
        {
            std::vector<TerrainPartDescriptor> parsedDescriptors;
            if (!ParseTerrainPartDescriptors(compositeBytes, parsedDescriptors))
            {
                AddWarning(outWarnings, "Failed to parse terrain composite payload: " + compositePath);
                terrainPartDescriptorsByPath.emplace(compositePath, std::vector<TerrainPartDescriptor>{});
                return;
            }

            terrainPartDescriptorsByPath.emplace(compositePath, std::move(parsedDescriptors));
        }

        const auto descriptorsIt = terrainPartDescriptorsByPath.find(compositePath);
        if (descriptorsIt == terrainPartDescriptorsByPath.end() || descriptorsIt->second.empty())
        {
            return;
        }

        for (const TerrainPartDescriptor& partDescriptor : descriptorsIt->second)
        {
            if (partDescriptor.AniPath.empty())
            {
                continue;
            }

            MapObjectRecord partObject;
            partObject.ObjectId        = nextObjectId++;
            partObject.ObjectType      = 2;
            partObject.RenderLayer     = MapLayer::Interactive;
            partObject.VisibilityFlags = 1u;
            // Terrain part scene offsets are tile-space deltas relative to the terrain root cell.
            // Keep the original values (no clamping) to avoid shifting multi-part scenes.
            partObject.TileX   = sourceObject.TileX + partDescriptor.SceneOffsetX;
            partObject.TileY   = sourceObject.TileY + partDescriptor.SceneOffsetY;
            partObject.OffsetX = partDescriptor.SpriteOffsetX;
            partObject.OffsetY = partDescriptor.SpriteOffsetY;
            // Legacy terrain-part "height" is not a reliable screen-space Z offset in practice.
            // Keep it out of sprite projection to prevent parts from being displaced off-screen.
            partObject.HeightOffset = 0;
            partObject.ResourcePath = partDescriptor.AniPath;
            partObject.ResourceTag  = partDescriptor.AniTag.empty() ? std::string("TerrainPart") : partDescriptor.AniTag;

            std::string resolvedAniPath;
            if (resolveMapObjectPath(partObject.ResourcePath, resolvedAniPath))
            {
                partObject.ResourcePath = resolvedAniPath;
            }

            resolveAniObjectToTexture(partObject);
            if (!partObject.ResourcePath.empty() && IsSupportedTexturePath(partObject.ResourcePath))
            {
                generatedMapObjects.push_back(std::move(partObject));
            }
        }
    };

    for (MapObjectRecord& objectRecord : outMapObjects)
    {
        if (objectRecord.ResourcePath.empty())
        {
            continue;
        }

        std::string resolvedObjectPath;
        if (!resolveMapObjectPath(objectRecord.ResourcePath, resolvedObjectPath))
        {
            continue;
        }

        objectRecord.ResourcePath = resolvedObjectPath;
        if (EndsWithInsensitive(resolvedObjectPath, ".ani"))
        {
            resolveAniObjectToTexture(objectRecord);
            continue;
        }

        if (EndsWithInsensitive(resolvedObjectPath, ".scene"))
        {
            const std::vector<uint8_t> sceneBytes = vfs.ReadAll(resolvedObjectPath);
            if (sceneBytes.empty())
            {
                AddWarning(outWarnings, "Failed to load map Scene resource: " + resolvedObjectPath);
                continue;
            }

            if (loadedSceneFiles.insert(resolvedObjectPath).second)
            {
                ++outDescriptor.LoadedSceneFileCount;
                ExtractEmbeddedPathHints(sceneBytes, pathHints);
                for (const std::string& hintPath : pathHints)
                {
                    std::string resolvedHintPath;
                    if (!resolveMapObjectPath(hintPath, resolvedHintPath))
                    {
                        continue;
                    }

                    if (EndsWithInsensitive(resolvedHintPath, ".part"))
                    {
                        registerScenePart(resolvedHintPath);
                    }
                }
            }

            if (objectRecord.ObjectType == static_cast<uint16_t>(kLegacyMapObjTerrain))
            {
                appendTerrainCompositeParts(objectRecord, resolvedObjectPath, sceneBytes);
                objectRecord.ResourcePath.clear();
            }

            continue;
        }

        if (EndsWithInsensitive(resolvedObjectPath, ".part"))
        {
            registerScenePart(resolvedObjectPath);
            if (objectRecord.ObjectType == static_cast<uint16_t>(kLegacyMapObjTerrain))
            {
                const std::vector<uint8_t> partBytes = vfs.ReadAll(resolvedObjectPath);
                if (!partBytes.empty())
                {
                    appendTerrainCompositeParts(objectRecord, resolvedObjectPath, partBytes);
                }

                objectRecord.ResourcePath.clear();
            }
            continue;
        }
    }

    if (!generatedMapObjects.empty())
    {
        outMapObjects.reserve(outMapObjects.size() + generatedMapObjects.size());
        for (MapObjectRecord& generatedObject : generatedMapObjects)
        {
            TileRecord* tileRecord = outTileGrid.GetTile(generatedObject.TileX, generatedObject.TileY);
            if (tileRecord != nullptr)
            {
                tileRecord->ObjectRefs.push_back(generatedObject.ObjectId);
            }

            outMapObjects.push_back(std::move(generatedObject));
        }
    }

    if (unresolvedAniTextureCount > 0)
    {
        std::ostringstream warningMessage;
        warningMessage << "Could not resolve ANI texture frames for " << unresolvedAniTextureCount << " map objects";
        if (!unresolvedAniSamples.empty())
        {
            warningMessage << " (sample: ";
            for (size_t sampleIndex = 0; sampleIndex < unresolvedAniSamples.size(); ++sampleIndex)
            {
                if (sampleIndex > 0)
                {
                    warningMessage << ", ";
                }
                warningMessage << unresolvedAniSamples[sampleIndex];
            }
            warningMessage << ")";
        }
        AddWarning(outWarnings, warningMessage.str());
    }

    if (cursor < bytes.size())
    {
        AddWarning(outWarnings,
                   "DMAP payload has trailing bytes (" + std::to_string(bytes.size() - cursor) + " bytes) after parsed sections");
    }

    uint64_t checksum = 1469598103934665603ull;
    for (uint8_t byteValue : bytes)
    {
        checksum ^= static_cast<uint64_t>(byteValue);
        checksum *= 1099511628211ull;
    }

    outDescriptor.LoadChecksum = checksum;
    outTileGrid.ComputeRowChecksums();
    return true;
}

bool MapLoader::ReadFixedBlock(const std::vector<uint8_t>& bytes,
                               size_t&                     ioCursor,
                               size_t                      blockBytes,
                               const char*                 blockName,
                               std::vector<uint8_t>&       outBlock,
                               std::vector<std::string>&   outWarnings)
{
    if (ioCursor + blockBytes > bytes.size())
    {
        AddWarning(outWarnings, std::string("DMAP payload truncated while reading ") + blockName);
        return false;
    }

    outBlock.resize(blockBytes);
    memcpy(outBlock.data(), bytes.data() + ioCursor, blockBytes);
    ioCursor += blockBytes;
    return true;
}

bool MapLoader::WorldToTile(const MapDescriptor& descriptor, int32_t worldX, int32_t worldY, int32_t& outTileX, int32_t& outTileY)
{
    if (descriptor.CellWidth == 0 || descriptor.CellHeight == 0 || descriptor.WidthInTiles == 0 || descriptor.HeightInTiles == 0)
    {
        return false;
    }

    const double normalizedX    = static_cast<double>(worldX - descriptor.OriginX);
    const double normalizedY    = static_cast<double>(worldY - descriptor.OriginY);
    const double halfCellWidth  = static_cast<double>(descriptor.CellWidth) * 0.5;
    const double halfCellHeight = static_cast<double>(descriptor.CellHeight) * 0.5;
    if (halfCellWidth <= 0.0 || halfCellHeight <= 0.0)
    {
        return false;
    }

    const double tileXF = (normalizedX / halfCellWidth + normalizedY / halfCellHeight) * 0.5;
    const double tileYF = (normalizedY / halfCellHeight - normalizedX / halfCellWidth) * 0.5;

    outTileX = std::clamp(static_cast<int32_t>(std::floor(tileXF)), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
    outTileY = std::clamp(static_cast<int32_t>(std::floor(tileYF)), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
    return true;
}

bool MapLoader::ParsePassageBlock(const std::vector<uint8_t>&    bytes,
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

bool MapLoader::ParseInteractiveObjectBlock(const std::vector<uint8_t>&   bytes,
                                            size_t&                       ioCursor,
                                            const MapDescriptor&          descriptor,
                                            std::vector<MapObjectRecord>& outMapObjects,
                                            uint32_t&                     ioObjectId,
                                            uint32_t&                     outParsedCount,
                                            uint32_t&                     outUnknownCount,
                                            bool&                         outAborted,
                                            std::vector<std::string>&     outWarnings)
{
    outParsedCount  = 0;
    outUnknownCount = 0;
    outAborted      = false;

    if (ioCursor + sizeof(uint32_t) > bytes.size())
    {
        AddWarning(outWarnings, "DMAP payload missing interactive object amount; skipping interactive object block");
        outAborted = true;
        return true;
    }

    const uint32_t objectCount = ReadU32(bytes, ioCursor);
    ioCursor += sizeof(uint32_t);
    std::vector<uint8_t> payload;
    uint32_t             alignmentRecoveries     = 0;
    constexpr uint32_t   kMaxAlignmentRecoveries = 16;
    size_t               coverRecordBytesHint    = 0;

    const auto assignWorldPosition = [&descriptor](MapObjectRecord& objectRecord, int32_t worldX, int32_t worldY)
    {
        objectRecord.WorldX           = worldX;
        objectRecord.WorldY           = worldY;
        objectRecord.HasWorldPosition = true;

        if (!WorldToTile(descriptor, worldX, worldY, objectRecord.TileX, objectRecord.TileY))
        {
            objectRecord.TileX = std::clamp(worldX, 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
            objectRecord.TileY = std::clamp(worldY, 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
        }
    };

    for (uint32_t objectIndex = 0; objectIndex < objectCount; ++objectIndex)
    {
        if (ioCursor + sizeof(uint32_t) > bytes.size())
        {
            AddWarning(outWarnings, "DMAP payload truncated while reading interactive object type; stopping interactive object parse");
            outAborted = true;
            break;
        }

        const int32_t objectType = ReadI32(bytes, ioCursor);
        ioCursor += sizeof(uint32_t);

        MapObjectRecord objectRecord;
        objectRecord.ObjectId    = ioObjectId++;
        objectRecord.ObjectType  = static_cast<uint16_t>(std::clamp<int32_t>(objectType, 0, std::numeric_limits<uint16_t>::max()));
        objectRecord.RenderLayer = MapLayer::Interactive;

        int32_t effectiveObjectType    = objectType;
        bool    retryWithRecoveredType = false;

        switch (effectiveObjectType)
        {
            case kLegacyMapObjCover:
            {
                constexpr size_t kCoverTileRecordBytes             = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 5;
                constexpr size_t kCoverTileRecordWithShowModeBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 6;
                constexpr size_t kCoverSceneRecordBytes            = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 7;

                if (coverRecordBytesHint == 0)
                {
                    size_t detectedRecordBytes = kCoverSceneRecordBytes;
                    if (objectIndex + 1 < objectCount)
                    {
                        const auto nextTypeLooksValid = [&](size_t candidateRecordBytes) -> bool
                        {
                            if (ioCursor + candidateRecordBytes + sizeof(uint32_t) > bytes.size())
                            {
                                return false;
                            }

                            return IsLikelyInteractiveObjectType(ReadI32(bytes, ioCursor + candidateRecordBytes));
                        };

                        if (nextTypeLooksValid(kCoverSceneRecordBytes))
                        {
                            detectedRecordBytes = kCoverSceneRecordBytes;
                        }
                        else if (nextTypeLooksValid(kCoverTileRecordWithShowModeBytes))
                        {
                            detectedRecordBytes = kCoverTileRecordWithShowModeBytes;
                        }
                        else if (nextTypeLooksValid(kCoverTileRecordBytes))
                        {
                            detectedRecordBytes = kCoverTileRecordBytes;
                        }
                    }

                    coverRecordBytesHint = detectedRecordBytes;
                }

                const size_t recordBytes = coverRecordBytesHint;
                if (!ReadFixedBlock(bytes, ioCursor, recordBytes, "MAP_COVER payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = ReadFixedCString(payload, kLegacyMapPathBytes, kLegacyMapTitleBytes);
                const size_t kFieldBase   = kLegacyMapPathBytes + kLegacyMapTitleBytes;

                const int32_t objectWorldX = ReadI32(payload, kFieldBase + 0);
                const int32_t objectWorldY = ReadI32(payload, kFieldBase + 4);
                assignWorldPosition(objectRecord, objectWorldX, objectWorldY);

                if (recordBytes == kCoverSceneRecordBytes)
                {
                    const int32_t frameWidth  = std::max(1, ReadI32(payload, kFieldBase + 8));
                    const int32_t frameHeight = std::max(1, ReadI32(payload, kFieldBase + 12));
                    objectRecord.OffsetX      = ReadI32(payload, kFieldBase + 16);
                    objectRecord.OffsetY      = ReadI32(payload, kFieldBase + 20);
                    objectRecord.FrameCount   = frameWidth * frameHeight;
                    objectRecord.ShowMode     = ReadI32(payload, kFieldBase + 24);
                }
                else
                {
                    objectRecord.OffsetX    = ReadI32(payload, kFieldBase + 8);
                    objectRecord.OffsetY    = ReadI32(payload, kFieldBase + 12);
                    objectRecord.FrameCount = ReadI32(payload, kFieldBase + 16);
                    if (recordBytes == kCoverTileRecordWithShowModeBytes)
                    {
                        objectRecord.ShowMode = ReadI32(payload, kFieldBase + 20);
                    }
                }

                objectRecord.VisibilityFlags = 1u;
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObjTerrain:
            {
                constexpr size_t kRecordBytes = kLegacyMapPathBytes + sizeof(int32_t) * 2;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_TERRAIN payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = "TerrainObject";
                assignWorldPosition(objectRecord, ReadI32(payload, kLegacyMapPathBytes + 0), ReadI32(payload, kLegacyMapPathBytes + 4));
                objectRecord.VisibilityFlags = 1u;
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObjScene:
            {
                constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 7;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_SCENE payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = ReadFixedCString(payload, kLegacyMapPathBytes, kLegacyMapTitleBytes);
                const size_t kFieldBase   = kLegacyMapPathBytes + kLegacyMapTitleBytes;
                assignWorldPosition(objectRecord, ReadI32(payload, kFieldBase + 0), ReadI32(payload, kFieldBase + 4));
                const int32_t frameWidth  = std::max(1, ReadI32(payload, kFieldBase + 8));
                const int32_t frameHeight = std::max(1, ReadI32(payload, kFieldBase + 12));
                objectRecord.OffsetX      = ReadI32(payload, kFieldBase + 16);
                objectRecord.OffsetY      = ReadI32(payload, kFieldBase + 20);
                objectRecord.FrameCount   = frameWidth * frameHeight;
                objectRecord.ShowMode     = ReadI32(payload, kFieldBase + 24);
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObjPuzzle:
            {
                constexpr size_t kRecordBytes = kLegacyMapPathBytes;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_PUZZLE payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = "PuzzleLayer";
                objectRecord.TileX        = static_cast<int32_t>(descriptor.WidthInTiles / 2u);
                objectRecord.TileY        = static_cast<int32_t>(descriptor.HeightInTiles / 2u);
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObjSound:
            {
                // Legacy maps store sound records without interval:
                //   path[260] + world(x,y) + range + volume = 276 bytes.
                // Some variants append interval; detect it by peeking the next object type.
                constexpr size_t kBaseRecordBytes = kLegacyMapPathBytes + sizeof(int32_t) * 4;
                if (ioCursor + kBaseRecordBytes > bytes.size())
                {
                    AddWarning(outWarnings, "DMAP payload truncated while reading MAP_SOUND payload");
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(bytes, ioCursor, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = "Sound";
                assignWorldPosition(
                    objectRecord, ReadI32(bytes, ioCursor + kLegacyMapPathBytes + 0), ReadI32(bytes, ioCursor + kLegacyMapPathBytes + 4));
                objectRecord.SoundRange      = std::max(0, ReadI32(bytes, ioCursor + kLegacyMapPathBytes + 8));
                objectRecord.SoundVolume     = std::max(0, ReadI32(bytes, ioCursor + kLegacyMapPathBytes + 12));
                objectRecord.VisibilityFlags = 1u;

                size_t consumedBytes = kBaseRecordBytes;
                if (objectIndex + 1 < objectCount && ioCursor + kBaseRecordBytes + sizeof(int32_t) <= bytes.size())
                {
                    const int32_t nextTypeWithoutInterval = ReadI32(bytes, ioCursor + kBaseRecordBytes);
                    const bool    looksWithoutInterval    = IsLikelyInteractiveObjectType(nextTypeWithoutInterval);
                    bool          looksWithInterval       = false;
                    if (ioCursor + kBaseRecordBytes + sizeof(int32_t) * 2 <= bytes.size())
                    {
                        const int32_t nextTypeWithInterval = ReadI32(bytes, ioCursor + kBaseRecordBytes + sizeof(int32_t));
                        looksWithInterval                  = IsLikelyInteractiveObjectType(nextTypeWithInterval);
                    }

                    if (!looksWithoutInterval && looksWithInterval)
                    {
                        objectRecord.SoundInterval = ReadI32(bytes, ioCursor + kBaseRecordBytes);
                        consumedBytes += sizeof(int32_t);
                    }
                }
                ioCursor += consumedBytes;

                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObj3DEffect:
            {
                constexpr size_t kRecordBytes = 64 + sizeof(int32_t) * 2;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_3DEFFECT payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourceTag = ReadFixedCString(payload, 0, 64);
                assignWorldPosition(objectRecord, ReadI32(payload, 64 + 0), ReadI32(payload, 64 + 4));
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObj3DEffectNew:
            {
                constexpr size_t kRecordBytes = 64 + sizeof(int32_t) * 2 + sizeof(float) * 6;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_3DEFFECTNEW payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourceTag = ReadFixedCString(payload, 0, 64);
                assignWorldPosition(objectRecord, ReadI32(payload, 64 + 0), ReadI32(payload, 64 + 4));
                for (int fi = 0; fi < 6; ++fi)
                {
                    objectRecord.EffectParams[fi] = ReadF32(payload, 64 + 8 + fi * sizeof(float));
                }
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            default:
            {
                // Legacy payloads may desynchronize by one int32 due optional fields.
                // Try to recover by re-aligning +/-4 bytes before giving up.
                const size_t typeOffset = ioCursor - sizeof(uint32_t);

                auto tryRecoverType = [&](int32_t deltaBytes) -> bool
                {
                    if (deltaBytes < 0 && typeOffset < static_cast<size_t>(-deltaBytes))
                    {
                        return false;
                    }

                    const size_t recoveredTypeOffset =
                        static_cast<size_t>(static_cast<int64_t>(typeOffset) + static_cast<int64_t>(deltaBytes));
                    if (recoveredTypeOffset + sizeof(uint32_t) > bytes.size())
                    {
                        return false;
                    }

                    const int32_t recoveredType = ReadI32(bytes, recoveredTypeOffset);
                    if (!IsLikelyInteractiveObjectType(recoveredType))
                    {
                        return false;
                    }

                    if (alignmentRecoveries >= kMaxAlignmentRecoveries)
                    {
                        return false;
                    }

                    effectiveObjectType = recoveredType;
                    objectRecord.ObjectType =
                        static_cast<uint16_t>(std::clamp<int32_t>(effectiveObjectType, 0, std::numeric_limits<uint16_t>::max()));
                    ioCursor = recoveredTypeOffset;
                    ++alignmentRecoveries;

                    AddWarning(outWarnings,
                               "Recovered interactive object parsing alignment by " + std::to_string(deltaBytes) +
                                   " bytes (type=" + std::to_string(effectiveObjectType) + ")");
                    retryWithRecoveredType = true;
                    return true;
                };

                if (!tryRecoverType(-static_cast<int32_t>(sizeof(uint32_t))) && !tryRecoverType(static_cast<int32_t>(sizeof(uint32_t))))
                {
                    AddWarning(outWarnings,
                               "Unsupported interactive object type " + std::to_string(objectType) +
                                   " in DMAP payload; stopping interactive object parse");
                    ++outUnknownCount;
                    outAborted = true;
                }
                break;
            }
        }

        if (retryWithRecoveredType)
        {
            --ioObjectId;
            --objectIndex;
            continue;
        }

        if (outAborted)
        {
            break;
        }
    }

    return true;
}

bool MapLoader::ParseSceneLayerBlock(const std::vector<uint8_t>&   bytes,
                                     size_t&                       ioCursor,
                                     const MapDescriptor&          descriptor,
                                     std::vector<MapObjectRecord>& outMapObjects,
                                     uint32_t&                     ioObjectId,
                                     uint32_t&                     outParsedCount,
                                     uint32_t&                     outUnknownCount,
                                     bool&                         outAborted,
                                     std::vector<std::string>&     outWarnings)
{
    outParsedCount  = 0;
    outUnknownCount = 0;
    outAborted      = false;

    if (ioCursor + sizeof(uint32_t) > bytes.size())
    {
        AddWarning(outWarnings, "DMAP payload missing extra-layer amount; skipping scene-layer payload");
        outAborted = true;
        return true;
    }

    const uint32_t extraLayerCount = ReadU32(bytes, ioCursor);
    ioCursor += sizeof(uint32_t);
    std::vector<uint8_t> payload;

    const auto assignWorldPosition = [&descriptor](MapObjectRecord& objectRecord, int32_t worldX, int32_t worldY)
    {
        objectRecord.WorldX           = worldX;
        objectRecord.WorldY           = worldY;
        objectRecord.HasWorldPosition = true;

        if (!WorldToTile(descriptor, worldX, worldY, objectRecord.TileX, objectRecord.TileY))
        {
            objectRecord.TileX = std::clamp(worldX, 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
            objectRecord.TileY = std::clamp(worldY, 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
        }
    };

    for (uint32_t layerOffset = 0; layerOffset < extraLayerCount; ++layerOffset)
    {
        if (ioCursor + sizeof(int32_t) * 2 > bytes.size())
        {
            AddWarning(outWarnings, "DMAP payload truncated while reading extra-layer header; stopping scene-layer parse");
            outAborted = true;
            break;
        }

        const int32_t layerIndex = ReadI32(bytes, ioCursor + 0);
        const int32_t layerType  = ReadI32(bytes, ioCursor + 4);
        ioCursor += sizeof(int32_t) * 2;

        if (layerType != kLegacyLayerScene)
        {
            AddWarning(outWarnings,
                       "Unsupported extra layer type " + std::to_string(layerType) + " in DMAP payload; stopping scene-layer parse");
            ++outUnknownCount;
            outAborted = true;
            break;
        }

        if (ioCursor + sizeof(int32_t) * 3 > bytes.size())
        {
            AddWarning(outWarnings, "DMAP payload truncated while reading scene-layer header; stopping scene-layer parse");
            outAborted = true;
            break;
        }

        const int32_t  moveRateX        = ReadI32(bytes, ioCursor + 0);
        const int32_t  moveRateY        = ReadI32(bytes, ioCursor + 4);
        const uint32_t sceneObjectCount = static_cast<uint32_t>(std::max(0, ReadI32(bytes, ioCursor + 8)));
        ioCursor += sizeof(int32_t) * 3;

        for (uint32_t sceneObjectIndex = 0; sceneObjectIndex < sceneObjectCount; ++sceneObjectIndex)
        {
            if (ioCursor + sizeof(uint32_t) > bytes.size())
            {
                AddWarning(outWarnings, "DMAP payload truncated while reading scene object type; stopping scene-layer parse");
                outAborted = true;
                break;
            }

            const int32_t sceneObjectType = ReadI32(bytes, ioCursor);
            ioCursor += sizeof(uint32_t);

            MapObjectRecord objectRecord;
            objectRecord.ObjectId    = ioObjectId++;
            objectRecord.ObjectType  = static_cast<uint16_t>(std::clamp<int32_t>(sceneObjectType, 0, std::numeric_limits<uint16_t>::max()));
            objectRecord.RenderLayer = MapLayer::Sky;
            objectRecord.VisibilityFlags = 1u;
            objectRecord.MoveRateX       = moveRateX;
            objectRecord.MoveRateY       = moveRateY;
            objectRecord.SceneLayerIndex = std::max(0, layerIndex);
            objectRecord.ResourceTag     = "Parallax(" + std::to_string(moveRateX) + "," + std::to_string(moveRateY) + ")";

            switch (sceneObjectType)
            {
                case kLegacyMapObjScene:
                case kLegacyMapObjCover:
                {
                    constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 7;
                    if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_SCENE payload", payload, outWarnings))
                    {
                        outAborted = true;
                        break;
                    }

                    objectRecord.ResourcePath    = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                    const std::string sceneTitle = ReadFixedCString(payload, kLegacyMapPathBytes, kLegacyMapTitleBytes);
                    if (!sceneTitle.empty())
                    {
                        objectRecord.ResourceTag += ":" + sceneTitle;
                    }

                    const size_t kFieldBase = kLegacyMapPathBytes + kLegacyMapTitleBytes;
                    assignWorldPosition(objectRecord, ReadI32(payload, kFieldBase + 0), ReadI32(payload, kFieldBase + 4));
                    const int32_t frameWidth  = std::max(1, ReadI32(payload, kFieldBase + 8));
                    const int32_t frameHeight = std::max(1, ReadI32(payload, kFieldBase + 12));
                    objectRecord.OffsetX      = ReadI32(payload, kFieldBase + 16);
                    objectRecord.OffsetY      = ReadI32(payload, kFieldBase + 20);
                    objectRecord.FrameCount   = frameWidth * frameHeight;
                    objectRecord.ShowMode     = ReadI32(payload, kFieldBase + 24);

                    ++outParsedCount;
                    outMapObjects.push_back(std::move(objectRecord));
                    break;
                }

                case kLegacyMapObjPuzzle:
                {
                    constexpr size_t kRecordBytes = kLegacyMapPathBytes;
                    if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_PUZZLE payload", payload, outWarnings))
                    {
                        outAborted = true;
                        break;
                    }

                    objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                    objectRecord.TileX        = static_cast<int32_t>(descriptor.WidthInTiles / 2u);
                    objectRecord.TileY        = static_cast<int32_t>(descriptor.HeightInTiles / 2u);
                    ++outParsedCount;
                    outMapObjects.push_back(std::move(objectRecord));
                    break;
                }

                default:
                    AddWarning(outWarnings,
                               "Unsupported scene object type " + std::to_string(sceneObjectType) +
                                   " in DMAP payload; stopping scene-layer parse");
                    ++outUnknownCount;
                    outAborted = true;
                    break;
            }

            if (outAborted)
            {
                break;
            }
        }

        if (outAborted)
        {
            break;
        }
    }

    return true;
}

bool MapLoader::ParsePuzzle(const std::string&                                      mapPath,
                            CVirtualFileSystem&                                     vfs,
                            TextureManager&                                         textureManager,
                            MapDescriptor&                                          inOutDescriptor,
                            TileGrid&                                               inOutTileGrid,
                            std::vector<MapAnimationState>&                         outAnimations,
                            std::unordered_map<uint16_t, std::shared_ptr<Texture>>& outTextureRefs,
                            std::vector<std::string>&                               outWarnings)
{
    std::string resolvedPuzzlePath;
    if (!inOutDescriptor.PuzzleAssetPath.empty() && vfs.Exists(inOutDescriptor.PuzzleAssetPath))
    {
        resolvedPuzzlePath = inOutDescriptor.PuzzleAssetPath;
    }

    if (resolvedPuzzlePath.empty())
    {
        const std::string sameDirPul = ReplaceExtension(mapPath, ".pul");
        if (vfs.Exists(sameDirPul))
        {
            resolvedPuzzlePath = sameDirPul;
        }
    }

    if (resolvedPuzzlePath.empty())
    {
        const std::string mapName            = BaseFileNameWithoutExtension(mapPath);
        const std::string standardPuzzlePath = "map/puzzle/" + mapName + ".pul";
        if (vfs.Exists(standardPuzzlePath))
        {
            resolvedPuzzlePath = standardPuzzlePath;
        }
    }

    if (resolvedPuzzlePath.empty())
    {
        AddWarning(outWarnings, "No puzzle file found for map (expected .pul)");
        return false;
    }

    const std::vector<uint8_t> bytes = vfs.ReadAll(resolvedPuzzlePath);
    if (bytes.size() < 272)
    {
        AddWarning(outWarnings, "Puzzle file too small: " + resolvedPuzzlePath);
        return false;
    }

    const std::string signature  = ReadFixedCString(bytes, 0, 8);
    const bool        isPuzzleV1 = signature == "PUZZLE";
    const bool        isPuzzleV2 = signature == "PUZZLE2";
    if (!isPuzzleV1 && !isPuzzleV2)
    {
        AddWarning(outWarnings, "Unsupported puzzle signature in " + resolvedPuzzlePath);
        return false;
    }

    const std::string aniPathRaw = ReadFixedCString(bytes, 8, 256);
    const uint32_t    gridWidth  = ReadU32(bytes, 264);
    const uint32_t    gridHeight = ReadU32(bytes, 268);

    if (gridWidth == 0 || gridHeight == 0)
    {
        AddWarning(outWarnings, "Puzzle grid dimensions are invalid");
        return false;
    }

    const uint64_t puzzleCount64 = static_cast<uint64_t>(gridWidth) * static_cast<uint64_t>(gridHeight);
    if (puzzleCount64 > std::numeric_limits<size_t>::max())
    {
        AddWarning(outWarnings, "Puzzle grid dimensions overflow host size limits");
        return false;
    }

    const size_t puzzleCount   = static_cast<size_t>(puzzleCount64);
    const size_t indicesOffset = 272;
    const size_t indicesBytes  = puzzleCount * sizeof(uint16_t);
    if (indicesOffset + indicesBytes > bytes.size())
    {
        AddWarning(outWarnings, "Puzzle index payload is incomplete");
        return false;
    }

    std::vector<uint16_t> puzzleIndices(puzzleCount, kInvalidPuzzleIndex);
    for (size_t i = 0; i < puzzleCount; ++i)
    {
        puzzleIndices[i] = ReadU16(bytes, indicesOffset + i * sizeof(uint16_t));
    }

    size_t cursor = indicesOffset + indicesBytes;
    if (isPuzzleV2)
    {
        if (cursor + 8 > bytes.size())
        {
            AddWarning(outWarnings, "Puzzle v2 file missing roll-speed fields");
            return false;
        }

        inOutDescriptor.PuzzleRollSpeedX = ReadI32(bytes, cursor);
        inOutDescriptor.PuzzleRollSpeedY = ReadI32(bytes, cursor + 4);
    }

    inOutDescriptor.PuzzleAssetPath  = resolvedPuzzlePath;
    inOutDescriptor.PuzzleAniPath    = NormalizeResourcePath(aniPathRaw);
    inOutDescriptor.PuzzleGridWidth  = gridWidth;
    inOutDescriptor.PuzzleGridHeight = gridHeight;
    inOutDescriptor.PuzzleIndices    = puzzleIndices;
    inOutDescriptor.MissingPuzzleIndices.clear();

    const uint32_t mapWidth  = inOutTileGrid.GetWidth();
    const uint32_t mapHeight = inOutTileGrid.GetHeight();
    if (mapWidth > 0 && mapHeight > 0 && inOutDescriptor.CellWidth > 0 && inOutDescriptor.CellHeight > 0)
    {
        const float cellW                  = static_cast<float>(inOutDescriptor.CellWidth);
        const float cellH                  = static_cast<float>(inOutDescriptor.CellHeight);
        const float originX                = static_cast<float>(inOutDescriptor.OriginX);
        const float originY                = static_cast<float>(inOutDescriptor.OriginY);
        const float bgWorldWidth           = static_cast<float>(gridWidth) * kPuzzleGridSize;
        const float bgWorldHeight          = static_cast<float>(gridHeight) * kPuzzleGridSize;
        const float mapHalfHeightWorld     = cellH * static_cast<float>(mapHeight) * 0.5f;
        const float legacyEvenHeightOffset = ((mapHeight + 1u) % 2u) != 0u ? cellH * 0.5f : 0.0f;
        const float bgWorldX               = originX - bgWorldWidth * 0.5f;
        const float bgWorldY               = originY + mapHalfHeightWorld - bgWorldHeight * 0.5f - legacyEvenHeightOffset;

        for (uint32_t y = 0; y < mapHeight; ++y)
        {
            for (uint32_t x = 0; x < mapWidth; ++x)
            {
                TileRecord* tile = inOutTileGrid.GetTile(static_cast<int32_t>(x), static_cast<int32_t>(y));
                if (tile == nullptr)
                {
                    continue;
                }

                const float   worldTileX = cellW * static_cast<float>(static_cast<int32_t>(x) - static_cast<int32_t>(y)) * 0.5f + originX;
                const float   worldTileY = cellH * static_cast<float>(static_cast<int32_t>(x) + static_cast<int32_t>(y)) * 0.5f + originY;
                const float   bgRelX     = worldTileX - bgWorldX;
                const float   bgRelY     = worldTileY - bgWorldY;
                const int32_t px =
                    std::clamp(static_cast<int32_t>(std::floor(bgRelX / kPuzzleGridSize)), 0, static_cast<int32_t>(gridWidth) - 1);
                const int32_t py =
                    std::clamp(static_cast<int32_t>(std::floor(bgRelY / kPuzzleGridSize)), 0, static_cast<int32_t>(gridHeight) - 1);
                tile->TextureId = puzzleIndices[static_cast<size_t>(py) * gridWidth + static_cast<size_t>(px)];
            }
        }
    }

    std::string resolvedAniPath = inOutDescriptor.PuzzleAniPath;
    if (resolvedAniPath.empty())
    {
        resolvedAniPath = BuildMapAniPath(mapPath);
    }

    if (!resolvedAniPath.empty() && !vfs.Exists(resolvedAniPath))
    {
        const std::string aniRelativeToPuzzle = JoinPath(resolvedPuzzlePath, resolvedAniPath);
        if (!aniRelativeToPuzzle.empty() && vfs.Exists(aniRelativeToPuzzle))
        {
            resolvedAniPath = aniRelativeToPuzzle;
        }

        const std::string baseAniPath = BuildMapAniPath(mapPath);
        if (!vfs.Exists(resolvedAniPath) && vfs.Exists(baseAniPath))
        {
            resolvedAniPath = baseAniPath;
        }
    }

    std::unordered_map<uint16_t, std::vector<std::string>> aniFramesByPuzzle;
    if (!resolvedAniPath.empty())
    {
        ParseAni(resolvedAniPath, vfs, aniFramesByPuzzle, outWarnings);
        inOutDescriptor.PuzzleAniPath = resolvedAniPath;
    }

    std::unordered_set<uint16_t> uniqueIndices;
    uniqueIndices.reserve(puzzleIndices.size());
    for (uint16_t value : puzzleIndices)
    {
        if (value == kInvalidPuzzleIndex)
        {
            continue;
        }

        uniqueIndices.insert(value);
    }

    std::vector<uint16_t> unresolvedPuzzleIndices;
    unresolvedPuzzleIndices.reserve(uniqueIndices.size());

    for (uint16_t puzzleIndex : uniqueIndices)
    {
        std::vector<std::shared_ptr<Texture>> frames;
        auto                                  framesIt = aniFramesByPuzzle.find(puzzleIndex);
        if (framesIt != aniFramesByPuzzle.end())
        {
            for (const std::string& framePathRaw : framesIt->second)
            {
                std::string resolvedFramePath;
                if (TryResolveTexturePath(framePathRaw, inOutDescriptor, vfs, resolvedFramePath))
                {
                    std::shared_ptr<Texture> frame = textureManager.LoadTexture(resolvedFramePath);
                    if (frame)
                    {
                        frames.push_back(frame);
                    }
                }
            }
        }

        if (frames.empty())
        {
            unresolvedPuzzleIndices.push_back(puzzleIndex);
            continue;
        }

        outTextureRefs[puzzleIndex] = frames.front();

        if (frames.size() > 1)
        {
            MapAnimationState state;
            state.AnimationId           = puzzleIndex;
            state.CurrentFrame          = 0;
            state.FrameStepMilliseconds = 160;
            state.Loop                  = true;
            state.Frames                = std::move(frames);
            outAnimations.push_back(std::move(state));
        }
    }

    if (!unresolvedPuzzleIndices.empty())
    {
        inOutDescriptor.MissingPuzzleIndices = unresolvedPuzzleIndices;
        std::ostringstream warningMessage;
        warningMessage << "Missing puzzle texture frames for " << unresolvedPuzzleIndices.size() << " puzzle indices";
        warningMessage << " (sample indices: ";
        const size_t sampleCount = std::min<size_t>(unresolvedPuzzleIndices.size(), 8);
        for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            if (sampleIndex > 0)
            {
                warningMessage << ", ";
            }

            warningMessage << unresolvedPuzzleIndices[sampleIndex];
        }

        if (unresolvedPuzzleIndices.size() > sampleCount)
        {
            warningMessage << ", ...";
        }
        warningMessage << ")";

        AddWarning(outWarnings, warningMessage.str());
    }

    return true;
}

bool MapLoader::ParseAni(const std::string&                                      aniPath,
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
                if (LooksLikeNumericMapId(idPart))
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

bool MapLoader::ResolveMapPath(const std::string&        dMapPath,
                               CVirtualFileSystem&       vfs,
                               std::string&              outResolvedPath,
                               uint32_t&                 outMapId,
                               uint32_t&                 outMapFlags,
                               std::vector<std::string>& outWarnings)
{
    outResolvedPath.clear();
    outMapId    = 0;
    outMapFlags = 0;

    const std::string normalizedRequested = NormalizeResourcePath(Trim(dMapPath));
    if (normalizedRequested.empty())
    {
        AddWarning(outWarnings, "Map path is empty");
        return false;
    }

    auto tryResolveExistingPath = [&outResolvedPath, &vfs](const std::string& rawPath) -> bool
    {
        const std::string normalizedPath = NormalizeResourcePath(rawPath);
        if (normalizedPath.empty())
        {
            return false;
        }

        if (vfs.Exists(normalizedPath))
        {
            outResolvedPath = normalizedPath;
            return true;
        }

        if (!EndsWithInsensitive(normalizedPath, ".dmap") && vfs.Exists(normalizedPath + ".dmap"))
        {
            outResolvedPath = normalizedPath + ".dmap";
            return true;
        }

        return false;
    };

    auto tryResolveMapAssetPath = [&tryResolveExistingPath](const std::string& rawPath) -> bool
    {
        const std::string normalizedPath = NormalizeResourcePath(rawPath);
        if (normalizedPath.empty())
        {
            return false;
        }

        const std::string loweredPath = ToLowerAscii(normalizedPath);
        if (loweredPath.rfind("map/", 0) == 0)
        {
            if (tryResolveExistingPath(normalizedPath))
            {
                return true;
            }

            return tryResolveExistingPath("data/" + normalizedPath);
        }

        if (loweredPath.rfind("data/map/", 0) == 0)
        {
            if (tryResolveExistingPath(normalizedPath))
            {
                return true;
            }

            return tryResolveExistingPath(normalizedPath.substr(5));
        }

        if (normalizedPath.find(':') != std::string::npos || (!normalizedPath.empty() && normalizedPath.front() == '/'))
        {
            return tryResolveExistingPath(normalizedPath);
        }

        if (tryResolveExistingPath("map/map/" + normalizedPath))
        {
            return true;
        }

        if (tryResolveExistingPath("data/map/map/" + normalizedPath))
        {
            return true;
        }

        return tryResolveExistingPath("data/map/" + normalizedPath);
    };

    if (tryResolveMapAssetPath(normalizedRequested))
    {
        return true;
    }

    if (LooksLikeNumericMapId(normalizedRequested))
    {
        uint32_t mapId = 0;
        try
        {
            mapId = static_cast<uint32_t>(std::stoul(normalizedRequested));
        }
        catch (const std::exception&)
        {
            mapId = 0;
        }

        if (mapId > 0)
        {
            std::vector<MapCatalogEntry> entries;
            if (ParseGameMapDat(vfs, entries, outWarnings))
            {
                for (const MapCatalogEntry& entry : entries)
                {
                    if (entry.MapId != mapId || entry.Path.empty())
                    {
                        continue;
                    }

                    if (!tryResolveMapAssetPath(entry.Path))
                    {
                        continue;
                    }

                    outMapId    = entry.MapId;
                    outMapFlags = entry.Flags;
                    return true;
                }
            }
        }
    }

    const std::string mapNameNoExt = BaseFileNameWithoutExtension(normalizedRequested);
    if (!mapNameNoExt.empty() && tryResolveMapAssetPath(mapNameNoExt + ".dmap"))
    {
        return true;
    }

    AddWarning(outWarnings, "Unable to resolve map path or map id: " + dMapPath);
    return false;
}

bool MapLoader::ParseGameMapDat(CVirtualFileSystem& vfs, std::vector<MapCatalogEntry>& outEntries, std::vector<std::string>& outWarnings)
{
    outEntries.clear();

    std::string datPath;
    if (vfs.Exists("ini/gamemap.dat"))
    {
        datPath = "ini/gamemap.dat";
    }
    else if (vfs.Exists("gamemap.dat"))
    {
        datPath = "gamemap.dat";
    }

    if (datPath.empty())
    {
        AddWarning(outWarnings, "GameMap.dat catalog not found in VFS");
        return false;
    }

    const std::vector<uint8_t> bytes = vfs.ReadAll(datPath);
    if (bytes.size() < sizeof(uint32_t))
    {
        AddWarning(outWarnings, "GameMap.dat is too small");
        return false;
    }

    auto parseAsBinary = [&]() -> bool
    {
        const uint32_t entryCount = ReadU32(bytes, 0);
        if (entryCount == 0 || entryCount > 100000)
        {
            return false;
        }

        size_t cursor = sizeof(uint32_t);
        outEntries.clear();
        outEntries.reserve(entryCount);

        for (uint32_t i = 0; i < entryCount; ++i)
        {
            if (cursor + 8 > bytes.size())
            {
                return false;
            }

            MapCatalogEntry entry;
            entry.MapId = ReadU32(bytes, cursor);
            cursor += 4;

            const uint32_t pathLength = ReadU32(bytes, cursor);
            cursor += 4;
            if (pathLength == 0 || pathLength > 4096 || cursor + static_cast<size_t>(pathLength) + 4 > bytes.size())
            {
                return false;
            }

            entry.Path = NormalizeResourcePath(std::string(reinterpret_cast<const char*>(bytes.data() + cursor), pathLength));
            cursor += pathLength;
            entry.Flags = ReadU32(bytes, cursor);
            cursor += 4;
            outEntries.push_back(std::move(entry));
        }

        return !outEntries.empty() && cursor == bytes.size();
    };

    auto parseAsText = [&]() -> bool
    {
        outEntries.clear();
        std::string text(bytes.begin(), bytes.end());
        std::replace(text.begin(), text.end(), '\r', '\n');

        std::istringstream stream(text);
        std::string        line;
        while (std::getline(stream, line))
        {
            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
            {
                continue;
            }

            std::string normalizedLine = trimmed;
            std::replace(normalizedLine.begin(), normalizedLine.end(), ',', ' ');
            std::replace(normalizedLine.begin(), normalizedLine.end(), '\t', ' ');

            std::istringstream lineStream(normalizedLine);
            std::string        mapIdToken;
            std::string        pathToken;
            std::string        flagsToken;
            lineStream >> mapIdToken >> pathToken >> flagsToken;
            if (mapIdToken.empty() || pathToken.empty() || !LooksLikeNumericMapId(mapIdToken))
            {
                continue;
            }

            MapCatalogEntry entry;
            try
            {
                entry.MapId = static_cast<uint32_t>(std::stoul(mapIdToken));
            }
            catch (const std::exception&)
            {
                continue;
            }
            if (!flagsToken.empty() && LooksLikeNumericMapId(flagsToken))
            {
                try
                {
                    entry.Flags = static_cast<uint32_t>(std::stoul(flagsToken));
                }
                catch (const std::exception&)
                {
                    entry.Flags = 0;
                }
            }

            entry.Path = NormalizeResourcePath(pathToken);
            if (entry.Path.empty())
            {
                continue;
            }

            outEntries.push_back(std::move(entry));
        }

        return !outEntries.empty();
    };

    if (parseAsBinary())
    {
        return true;
    }

    if (parseAsText())
    {
        AddWarning(outWarnings, "GameMap.dat parsed using text fallback mode");
        return true;
    }

    AddWarning(outWarnings, "GameMap.dat parse failed (unsupported format)");
    return false;
}

bool MapLoader::LooksLikeNumericMapId(const std::string& value)
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

std::string MapLoader::Trim(std::string value)
{
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return {};
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string MapLoader::NormalizeResourcePath(const std::string& value)
{
    return NormalizeVirtualResourcePath(value, true);
}

std::string MapLoader::JoinPath(const std::string& basePath, const std::string& relativePath)
{
    const std::string normalizedRelative = NormalizeResourcePath(relativePath);
    if (normalizedRelative.empty())
    {
        return {};
    }

    if (normalizedRelative.find(':') != std::string::npos || (!normalizedRelative.empty() && normalizedRelative.front() == '/'))
    {
        return normalizedRelative;
    }

    const size_t slashPos = basePath.find_last_of('/');
    if (slashPos == std::string::npos)
    {
        return normalizedRelative;
    }

    return NormalizeResourcePath(basePath.substr(0, slashPos + 1) + normalizedRelative);
}

std::string MapLoader::BaseFileNameWithoutExtension(const std::string& path)
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

std::string MapLoader::BuildMapAniPath(const std::string& mapPath)
{
    const std::string mapName = BaseFileNameWithoutExtension(mapPath);
    if (mapName.empty())
    {
        return {};
    }

    return "ani/" + mapName + ".ani";
}

bool MapLoader::TryResolveTexturePath(const std::string&   framePath,
                                      const MapDescriptor& descriptor,
                                      CVirtualFileSystem&  vfs,
                                      std::string&         outResolvedPath)
{
    outResolvedPath.clear();

    const std::string normalizedFramePath = NormalizeResourcePath(framePath);
    if (normalizedFramePath.empty())
    {
        return false;
    }

    auto tryTexturePath = [&outResolvedPath, &vfs](const std::string& rawCandidate) -> bool
    {
        const std::string normalizedCandidate = NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return false;
        }

        const std::string loweredCandidate = ToLowerAscii(normalizedCandidate);
        if (loweredCandidate.rfind("map/", 0) != 0 && loweredCandidate.rfind("data/map/", 0) != 0)
        {
            return false;
        }

        if (!IsSupportedTexturePath(normalizedCandidate))
        {
            return false;
        }

        if (!vfs.Exists(normalizedCandidate))
        {
            return false;
        }

        const std::vector<uint8_t> bytes = vfs.ReadAll(normalizedCandidate);
        if (bytes.empty() || !LooksLikeTexturePayload(normalizedCandidate, bytes))
        {
            return false;
        }

        outResolvedPath = normalizedCandidate;
        return true;
    };

    auto tryTextureWithExtension = [&tryTexturePath](const std::string& rawBasePath) -> bool
    {
        const std::string normalizedBasePath = NormalizeVirtualResourcePath(rawBasePath, true);
        if (normalizedBasePath.empty())
        {
            return false;
        }

        const size_t dotPos = normalizedBasePath.find_last_of('.');
        if (dotPos == std::string::npos)
        {
            return tryTexturePath(normalizedBasePath + ".dds") || tryTexturePath(normalizedBasePath + ".tga");
        }

        const std::string extension = ToLowerAscii(normalizedBasePath.substr(dotPos));
        if (extension == ".dds")
        {
            return tryTexturePath(normalizedBasePath) || tryTexturePath(normalizedBasePath.substr(0, dotPos) + ".tga");
        }

        if (extension == ".tga")
        {
            return tryTexturePath(normalizedBasePath) || tryTexturePath(normalizedBasePath.substr(0, dotPos) + ".dds");
        }

        const std::string stem = normalizedBasePath.substr(0, dotPos);
        return tryTexturePath(stem + ".dds") || tryTexturePath(stem + ".tga");
    };

    if (IsVirtualRootedPath(normalizedFramePath))
    {
        if (tryTextureWithExtension(normalizedFramePath))
        {
            return true;
        }

        const std::string loweredFramePath = ToLowerAscii(normalizedFramePath);
        if (loweredFramePath.rfind("map/", 0) == 0)
        {
            return tryTextureWithExtension("data/" + normalizedFramePath);
        }

        if (loweredFramePath.rfind("data/map/", 0) == 0)
        {
            return tryTextureWithExtension(normalizedFramePath.substr(5));
        }

        return false;
    }

    const std::string puzzleRelativePath = JoinPath(descriptor.PuzzleAssetPath, normalizedFramePath);
    if (!puzzleRelativePath.empty() && tryTextureWithExtension(puzzleRelativePath))
    {
        return true;
    }

    if (tryTextureWithExtension("map/" + normalizedFramePath))
    {
        return true;
    }

    return tryTextureWithExtension("data/map/" + normalizedFramePath);
}

uint16_t MapLoader::ReadU16(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint16_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

int16_t MapLoader::ReadI16(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int16_t>(ReadU16(bytes, offset));
}

uint32_t MapLoader::ReadU32(const std::vector<uint8_t>& bytes, size_t offset)
{
    if (offset + sizeof(uint32_t) > bytes.size())
    {
        return 0;
    }

    return static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

int32_t MapLoader::ReadI32(const std::vector<uint8_t>& bytes, size_t offset)
{
    return static_cast<int32_t>(ReadU32(bytes, offset));
}

std::string MapLoader::ReadFixedCString(const std::vector<uint8_t>& bytes, size_t offset, size_t length)
{
    if (offset >= bytes.size() || length == 0)
    {
        return {};
    }

    const size_t end       = std::min(bytes.size(), offset + length);
    size_t       actualEnd = offset;
    while (actualEnd < end && bytes[actualEnd] != 0)
    {
        ++actualEnd;
    }

    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), actualEnd - offset);
}

std::string MapLoader::ReplaceExtension(const std::string& path, const std::string& extension)
{
    const size_t dotIndex = path.find_last_of('.');
    if (dotIndex == std::string::npos)
    {
        return path + extension;
    }

    return path.substr(0, dotIndex) + extension;
}

void MapLoader::AddWarning(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LX_MAP_WARN("[Map] {}", message);
}

} // namespace LongXi
