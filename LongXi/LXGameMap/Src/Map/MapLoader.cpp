#include "Map/MapLoader.h"

#include <algorithm>
#include <atomic>
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
#include "Core/StringUtils.h"
#include "Map/AniParser.h"
#include "Map/DmapObjectParser.h"
#include "Map/DmapParser.h"
#include "Map/MapBinaryReader.h"
#include "Map/PuzzleParser.h"
#include "Map/TileGrid.h"
#include "Texture/Texture.h"
#include "Texture/TextureManager.h"

namespace LXMap
{

namespace
{
constexpr int32_t kLegacyLayerScene = 4;

bool IsSupportedTexturePath(const std::string& path)
{
    return LXCore::EndsWithInsensitive(path, ".dds") || LXCore::EndsWithInsensitive(path, ".tga");
}

bool IsVirtualRootedPath(const std::string& path)
{
    const std::string normalizedPath = LXCore::ToLowerAscii(LXCore::NormalizeVirtualResourcePath(path, true));
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
    if (LXCore::EndsWithInsensitive(normalizedPath, ".dds"))
    {
        return bytes.size() >= 4 && bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S' && bytes[3] == ' ';
    }

    if (LXCore::EndsWithInsensitive(normalizedPath, ".tga"))
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

std::string NormalizeCatalogDmapPath(const std::string& rawPath)
{
    std::string normalizedPath = LXCore::NormalizeVirtualResourcePath(rawPath, true);
    if (normalizedPath.empty())
    {
        return {};
    }

    if (!LXCore::EndsWithInsensitive(normalizedPath, ".dmap"))
    {
        normalizedPath += ".dmap";
    }

    const std::string loweredPath = LXCore::ToLowerAscii(normalizedPath);
    if (loweredPath.rfind("map/map/", 0) == 0)
    {
        return normalizedPath;
    }

    if (loweredPath.rfind("map/", 0) == 0)
    {
        return "map/" + normalizedPath;
    }

    return "map/map/" + normalizedPath;
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

std::string StripArchivePrefix(std::string stem)
{
    if (stem.size() > 2 && std::isalpha(static_cast<unsigned char>(stem[0])) != 0 && (stem[1] == '-' || stem[1] == '_'))
    {
        return stem.substr(2);
    }

    return stem;
}

bool CatalogPathMatchesResolvedDmap(const std::string& catalogPath, const std::string& resolvedPath)
{
    const std::string normalizedResolved = LXCore::NormalizeVirtualResourcePath(resolvedPath, true);
    const std::string normalizedCatalog  = LXCore::NormalizeVirtualResourcePath(catalogPath, true);
    if (normalizedResolved.empty() || normalizedCatalog.empty())
    {
        return false;
    }

    if (LXCore::EndsWithInsensitive(normalizedCatalog, ".dmap") && NormalizeCatalogDmapPath(normalizedCatalog) == normalizedResolved)
    {
        return true;
    }

    const std::string catalogStem  = LXCore::ToLowerAscii(StripArchivePrefix(FileStem(normalizedCatalog)));
    const std::string resolvedStem = LXCore::ToLowerAscii(FileStem(normalizedResolved));
    return !catalogStem.empty() && catalogStem == resolvedStem;
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

    size_t cursor = 0;
    if (!ParseDmap(mapPath, bytes, outDescriptor, outTileGrid, cursor, outWarnings))
    {
        return false;
    }

    uint32_t nextObjectId              = 1;
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
    using AniSectionFrames = std::unordered_map<std::string, std::vector<std::string>>;
    std::unordered_map<std::string, AniSectionFrames> aniSectionsByPath;

    auto resolveMapObjectPath = [&vfs](const std::string& rawPath, std::string& outResolvedPath) -> bool
    {
        outResolvedPath.clear();

        const std::string normalizedRaw = LXCore::NormalizeVirtualResourcePath(rawPath, true);
        if (normalizedRaw.empty())
        {
            return false;
        }

        auto tryPath = [&outResolvedPath, &vfs](const std::string& rawCandidate) -> bool
        {
            const std::string normalizedCandidate = LXCore::NormalizeVirtualResourcePath(rawCandidate, true);
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

        const std::string loweredRaw = LXCore::ToLowerAscii(normalizedRaw);
        if (loweredRaw.rfind("ani/", 0) == 0)
        {
            return tryPath(normalizedRaw);
        }

        if (loweredRaw.rfind("map/", 0) == 0)
        {
            return tryPath(normalizedRaw);
        }

        if (loweredRaw.rfind("data/map/", 0) == 0)
        {
            return tryPath(normalizedRaw);
        }

        const std::string fileName  = FileNameFromPath(normalizedRaw);
        const size_t      dotPos    = fileName.find_last_of('.');
        const std::string extension = (dotPos == std::string::npos) ? std::string() : LXCore::ToLowerAscii(fileName.substr(dotPos));

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
            return tryPath("map/scene/" + fileName);
        }
        if (extension == ".part")
        {
            return tryPath("map/scenepart/" + fileName);
        }

        return tryPath(normalizedRaw);
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
                    currentSection = LXCore::ToLowerAscii(Trim(trimmed.substr(1, trimmed.size() - 2)));
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

                key = LXCore::ToLowerAscii(key);
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

        static std::atomic<uint32_t> s_AniResolveFailureLogCount{0};

        const AniSectionFrames* sections = loadAniSections(aniPath);
        if (sections == nullptr || sections->empty())
        {
            if (s_AniResolveFailureLogCount.load(std::memory_order_relaxed) < 8)
            {
                LX_MAP_WARN("[Map] ANI resolve failed: no sections loaded for '{}' (tag='{}')", aniPath, resourceTag);
                s_AniResolveFailureLogCount.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        std::vector<std::string> sectionCandidates;
        bool                     matchedSection = false;
        std::vector<std::string> sampledFramePaths;
        auto                     pushSectionCandidate = [&sectionCandidates](const std::string& rawValue)
        {
            std::string normalized = LXCore::ToLowerAscii(Trim(rawValue));
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

            matchedSection = true;

            for (const std::string& framePath : sectionIt->second)
            {
                if (sampledFramePaths.size() < 3)
                {
                    sampledFramePaths.push_back(framePath);
                }

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

        if (s_AniResolveFailureLogCount.load(std::memory_order_relaxed) < 8)
        {
            std::ostringstream message;
            message << "ANI resolve failed: ani='" << aniPath << "' tag='" << resourceTag
                    << "' matchedSection=" << (matchedSection ? "true" : "false");
            if (!sectionCandidates.empty())
            {
                message << " candidates=";
                for (size_t index = 0; index < sectionCandidates.size(); ++index)
                {
                    if (index > 0)
                    {
                        message << "|";
                    }
                    message << sectionCandidates[index];
                }
            }

            if (!sampledFramePaths.empty())
            {
                message << " frameSample=";
                for (size_t index = 0; index < sampledFramePaths.size(); ++index)
                {
                    if (index > 0)
                    {
                        message << "|";
                    }
                    message << sampledFramePaths[index];
                }
            }

            LX_MAP_WARN("[Map] {}", message.str());
            s_AniResolveFailureLogCount.fetch_add(1, std::memory_order_relaxed);
        }

        return false;
    };

    auto registerScenePart = [&vfs, &loadedScenePartFiles, &outDescriptor, &outWarnings](
                                 const std::string& partPath, const std::vector<uint8_t>* partBytes = nullptr) -> bool
    {
        if (!loadedScenePartFiles.insert(partPath).second)
        {
            return true;
        }

        if (partBytes != nullptr)
        {
            if (partBytes->empty())
            {
                AddWarning(outWarnings, "Failed to load map ScenePart resource: " + partPath);
                return false;
            }
        }
        else
        {
            const std::vector<uint8_t> loadedBytes = vfs.ReadAll(partPath);
            if (loadedBytes.empty())
            {
                AddWarning(outWarnings, "Failed to load map ScenePart resource: " + partPath);
                return false;
            }
        }

        ++outDescriptor.LoadedScenePartCount;
        return true;
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

        if (objectRecord.ResourcePath.empty() || !LXCore::EndsWithInsensitive(objectRecord.ResourcePath, ".ani"))
        {
            return;
        }

        // Preserve ANI source path and section name before resolving to a single frame texture.
        objectRecord.AniSourcePath  = objectRecord.ResourcePath;
        objectRecord.AniSectionName = objectRecord.ResourceTag;

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

    uint32_t terrainPartOutOfBoundsWarningCount = 0;

    auto appendTerrainCompositeParts = [&nextObjectId,
                                        &terrainPartDescriptorsByPath,
                                        &resolveMapObjectPath,
                                        &resolveAniObjectToTexture,
                                        &generatedMapObjects,
                                        &outDescriptor,
                                        &outWarnings,
                                        &terrainPartOutOfBoundsWarningCount](const MapObjectRecord&      sourceObject,
                                                                             const std::string&          compositePath,
                                                                             const std::vector<uint8_t>& compositeBytes) -> bool
    {
        const auto cacheIt = terrainPartDescriptorsByPath.find(compositePath);
        if (cacheIt == terrainPartDescriptorsByPath.end())
        {
            std::vector<TerrainPartDescriptor> parsedDescriptors;
            if (!ParseTerrainPartDescriptors(compositeBytes, parsedDescriptors))
            {
                AddWarning(outWarnings, "Failed to parse terrain composite payload: " + compositePath);
                terrainPartDescriptorsByPath.emplace(compositePath, std::vector<TerrainPartDescriptor>{});
                return false;
            }

            terrainPartDescriptorsByPath.emplace(compositePath, std::move(parsedDescriptors));
        }

        const auto descriptorsIt = terrainPartDescriptorsByPath.find(compositePath);
        if (descriptorsIt == terrainPartDescriptorsByPath.end() || descriptorsIt->second.empty())
        {
            return false;
        }

        bool appendedAnyPart = false;
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
            // SceneOffset values are already in tile coordinates, not pixels.
            // The old client adds them directly to the source object's tile position.
            partObject.TileX = sourceObject.TileX + partDescriptor.SceneOffsetX;
            partObject.TileY = sourceObject.TileY + partDescriptor.SceneOffsetY;
            if ((partObject.TileX < 0 || partObject.TileY < 0 || partObject.TileX >= static_cast<int32_t>(outDescriptor.WidthInTiles) ||
                 partObject.TileY >= static_cast<int32_t>(outDescriptor.HeightInTiles)) &&
                terrainPartOutOfBoundsWarningCount < 8)
            {
                AddWarning(outWarnings,
                           "Terrain part anchor is outside map bounds: " + compositePath + " tile=(" + std::to_string(partObject.TileX) +
                               "," + std::to_string(partObject.TileY) + ")");
                ++terrainPartOutOfBoundsWarningCount;
            }
            partObject.BaseWidth     = std::max(1, partDescriptor.BaseWidth);
            partObject.BaseHeight    = std::max(1, partDescriptor.BaseHeight);
            partObject.OffsetX       = partDescriptor.SpriteOffsetX;
            partObject.OffsetY       = partDescriptor.SpriteOffsetY;
            partObject.FrameInterval = std::max(1, partDescriptor.FrameInterval);
            partObject.ShowMode      = partDescriptor.ShowType;
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
                appendedAnyPart = true;
            }
        }

        return appendedAnyPart;
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
        if (LXCore::EndsWithInsensitive(resolvedObjectPath, ".ani"))
        {
            resolveAniObjectToTexture(objectRecord);
            continue;
        }

        if (LXCore::EndsWithInsensitive(resolvedObjectPath, ".scene"))
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
            }

            if (appendTerrainCompositeParts(objectRecord, resolvedObjectPath, sceneBytes))
            {
                objectRecord.ResourcePath.clear();
            }

            continue;
        }

        if (LXCore::EndsWithInsensitive(resolvedObjectPath, ".part"))
        {
            const std::vector<uint8_t> partBytes = vfs.ReadAll(resolvedObjectPath);
            if (!registerScenePart(resolvedObjectPath, &partBytes))
            {
                continue;
            }

            if (appendTerrainCompositeParts(objectRecord, resolvedObjectPath, partBytes))
            {
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

        if (!LXCore::EndsWithInsensitive(normalizedPath, ".dmap") && vfs.Exists(normalizedPath + ".dmap"))
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

        if (normalizedPath.find(':') != std::string::npos || (!normalizedPath.empty() && normalizedPath.front() == '/'))
        {
            return tryResolveExistingPath(normalizedPath);
        }

        const std::string loweredPath = LXCore::ToLowerAscii(normalizedPath);
        if (loweredPath.rfind("map/map/", 0) == 0)
        {
            return tryResolveExistingPath(normalizedPath);
        }

        if (loweredPath.rfind("map/", 0) == 0)
        {
            return tryResolveExistingPath("map/" + normalizedPath);
        }

        return tryResolveExistingPath("map/map/" + normalizedPath);
    };

    auto applyCatalogMetadataForResolvedPath = [&vfs, &outResolvedPath, &outMapId, &outMapFlags, &outWarnings]()
    {
        if (outResolvedPath.empty())
        {
            return;
        }

        std::vector<MapCatalogEntry> entries;
        if (!ParseGameMapDat(vfs, entries, outWarnings))
        {
            return;
        }

        for (const MapCatalogEntry& entry : entries)
        {
            if (entry.Path.empty())
            {
                continue;
            }

            if (CatalogPathMatchesResolvedDmap(entry.Path, outResolvedPath))
            {
                outMapId    = entry.MapId;
                outMapFlags = entry.Flags;
                return;
            }
        }
    };

    if (tryResolveMapAssetPath(normalizedRequested))
    {
        applyCatalogMetadataForResolvedPath();
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
        applyCatalogMetadataForResolvedPath();
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
    return LXCore::NormalizeVirtualResourcePath(value, true);
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
        const std::string normalizedCandidate = LXCore::NormalizeVirtualResourcePath(rawCandidate, true);
        if (normalizedCandidate.empty())
        {
            return false;
        }

        const std::string loweredCandidate = LXCore::ToLowerAscii(normalizedCandidate);
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
        const std::string normalizedBasePath = LXCore::NormalizeVirtualResourcePath(rawBasePath, true);
        if (normalizedBasePath.empty())
        {
            return false;
        }

        const size_t dotPos = normalizedBasePath.find_last_of('.');
        if (dotPos == std::string::npos)
        {
            return tryTexturePath(normalizedBasePath + ".dds") || tryTexturePath(normalizedBasePath + ".tga");
        }

        const std::string extension = LXCore::ToLowerAscii(normalizedBasePath.substr(dotPos));
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

} // namespace LXMap
