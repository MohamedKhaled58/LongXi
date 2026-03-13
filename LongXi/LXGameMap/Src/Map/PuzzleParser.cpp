#include "Map/PuzzleParser.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/FileSystem/PathUtils.h"
#include "Core/FileSystem/VirtualFileSystem.h"
#include "Core/Logging/LogMacros.h"
#include "Core/StringUtils.h"
#include "Map/AniParser.h"
#include "Map/MapBinaryReader.h"
#include "Map/MapTypes.h"
#include "Map/TileGrid.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LongXi
{

namespace
{

void AddWarning(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LX_MAP_WARN("[Map] {}", message);
}

std::string NormalizeResourcePath(const std::string& value)
{
    return NormalizeVirtualResourcePath(value, true);
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

std::string BuildMapAniPath(const std::string& mapPath)
{
    const std::string mapName = BaseFileNameWithoutExtension(mapPath);
    if (mapName.empty())
    {
        return {};
    }

    return "ani/" + mapName + ".ani";
}

std::string JoinPath(const std::string& basePath, const std::string& relativePath)
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

bool TryResolveTexturePath(const std::string&   framePath,
                           const MapDescriptor& descriptor,
                           CVirtualFileSystem&  vfs,
                           std::string&         outResolvedPath)
{
    outResolvedPath.clear();

    static std::atomic<uint32_t> s_TextureResolveFailureLogCount{0};
    auto logTextureResolveFailure = [](const std::string& candidatePath, const char* reason, const std::vector<uint8_t>* bytes = nullptr)
    {
        if (s_TextureResolveFailureLogCount.load(std::memory_order_relaxed) >= 8)
        {
            return;
        }

        if (bytes != nullptr && !bytes->empty())
        {
            const uint8_t b0 = (*bytes)[0];
            const uint8_t b1 = (bytes->size() > 1) ? (*bytes)[1] : 0;
            const uint8_t b2 = (bytes->size() > 2) ? (*bytes)[2] : 0;
            const uint8_t b3 = (bytes->size() > 3) ? (*bytes)[3] : 0;
            LX_MAP_WARN("[Map] Texture resolve failed at {} for '{}' (size={}, firstBytes={:02X} {:02X} {:02X} {:02X})",
                        reason,
                        candidatePath,
                        bytes->size(),
                        b0,
                        b1,
                        b2,
                        b3);
        }
        else
        {
            LX_MAP_WARN("[Map] Texture resolve failed at {} for '{}'", reason, candidatePath);
        }

        s_TextureResolveFailureLogCount.fetch_add(1, std::memory_order_relaxed);
    };

    const std::string normalizedFramePath = NormalizeResourcePath(framePath);
    if (normalizedFramePath.empty())
    {
        return false;
    }

    auto tryTexturePath = [&outResolvedPath, &vfs, &logTextureResolveFailure](const std::string& rawCandidate) -> bool
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
            logTextureResolveFailure(normalizedCandidate, "vfs.Exists");
            return false;
        }

        const std::vector<uint8_t> bytes = vfs.ReadAll(normalizedCandidate);
        if (bytes.empty())
        {
            logTextureResolveFailure(normalizedCandidate, "vfs.ReadAll-empty");
            return false;
        }

        if (!LooksLikeTexturePayload(normalizedCandidate, bytes))
        {
            logTextureResolveFailure(normalizedCandidate, "payload-check", &bytes);
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

} // namespace

bool ParsePuzzle(const std::string&                                      mapPath,
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
        const float cellW          = static_cast<float>(inOutDescriptor.CellWidth);
        const float cellH          = static_cast<float>(inOutDescriptor.CellHeight);
        const float originX        = static_cast<float>(inOutDescriptor.OriginX);
        const float originY        = static_cast<float>(inOutDescriptor.OriginY);
        const float bgWorldWidth   = static_cast<float>(gridWidth) * kPuzzleGridSize;
        const float bgWorldHeight  = static_cast<float>(gridHeight) * kPuzzleGridSize;
        const float bgCenterWorldY = cellH * static_cast<float>(mapHeight) * 0.5f;
        const float bgWorldX       = originX - bgWorldWidth * 0.5f;
        const float bgWorldY       = bgCenterWorldY - bgWorldHeight * 0.5f;

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

} // namespace LongXi
