#include "Map/DmapObjectParser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include "Core/FileSystem/PathUtils.h"
#include "Core/Logging/LogMacros.h"
#include "Map/MapBinaryReader.h"
#include "Map/MapTypes.h"

namespace LXMap
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

void AddWarning(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LX_MAP_WARN("[Map] {}", message);
}

std::string NormalizeResourcePath(const std::string& value)
{
    return LXCore::NormalizeVirtualResourcePath(value, true);
}

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

bool WorldToTile(const MapDescriptor& descriptor, int32_t worldX, int32_t worldY, int32_t& outTileX, int32_t& outTileY)
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

bool ReadFixedBlock(const std::vector<uint8_t>& bytes,
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

} // namespace

bool ParseInteractiveObjectBlock(const std::vector<uint8_t>&   bytes,
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
                // Legacy MAP_COVER binary layout is fixed at 416 bytes after type:
                // path[260] + title[128] + cell(x,y) + base(w,h) + offset(x,y) + frameInterval.
                constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 7;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_COVER payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = ReadFixedCString(payload, kLegacyMapPathBytes, kLegacyMapTitleBytes);
                const size_t kFieldBase   = kLegacyMapPathBytes + kLegacyMapTitleBytes;

                objectRecord.TileX = std::clamp(ReadI32(payload, kFieldBase + 0), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
                objectRecord.TileY = std::clamp(ReadI32(payload, kFieldBase + 4), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
                objectRecord.BaseWidth       = std::max(1, ReadI32(payload, kFieldBase + 8));
                objectRecord.BaseHeight      = std::max(1, ReadI32(payload, kFieldBase + 12));
                objectRecord.OffsetX         = ReadI32(payload, kFieldBase + 16);
                objectRecord.OffsetY         = ReadI32(payload, kFieldBase + 20);
                objectRecord.FrameInterval   = std::max(1, ReadI32(payload, kFieldBase + 24));
                objectRecord.FrameCount      = 0;
                objectRecord.ShowMode        = 0;
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
                objectRecord.TileX =
                    std::clamp(ReadI32(payload, kLegacyMapPathBytes + 0), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
                objectRecord.TileY =
                    std::clamp(ReadI32(payload, kLegacyMapPathBytes + 4), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
                objectRecord.VisibilityFlags = 1u;
                ++outParsedCount;
                outMapObjects.push_back(std::move(objectRecord));
                break;
            }

            case kLegacyMapObjScene:
            {
                // Legacy MAP_SCENE binary layout is fixed at 412 bytes after type:
                // path[260] + title[128] + cell(x,y) + offset(x,y) + frameInterval + showWay.
                constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 6;
                if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_SCENE payload", payload, outWarnings))
                {
                    outAborted = true;
                    break;
                }

                objectRecord.ResourcePath = NormalizeResourcePath(ReadFixedCString(payload, 0, kLegacyMapPathBytes));
                objectRecord.ResourceTag  = ReadFixedCString(payload, kLegacyMapPathBytes, kLegacyMapTitleBytes);
                const size_t kFieldBase   = kLegacyMapPathBytes + kLegacyMapTitleBytes;
                objectRecord.TileX = std::clamp(ReadI32(payload, kFieldBase + 0), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
                objectRecord.TileY = std::clamp(ReadI32(payload, kFieldBase + 4), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
                // MAP_SCENE layout: cellX(+0), cellY(+4), offsetX(+8), offsetY(+12), frameInterval(+16), showType(+20)
                objectRecord.OffsetX       = ReadI32(payload, kFieldBase + 8);
                objectRecord.OffsetY       = ReadI32(payload, kFieldBase + 12);
                objectRecord.FrameInterval = std::max(1, ReadI32(payload, kFieldBase + 16));
                objectRecord.ShowMode      = ReadI32(payload, kFieldBase + 20);
                objectRecord.FrameCount    = 0;

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
                // Legacy payloads may desynchronize by one int32 due to optional fields.
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

bool ParseSceneLayerBlock(const std::vector<uint8_t>&   bytes,
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
                {
                    constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 6;
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
                    objectRecord.TileX = std::clamp(ReadI32(payload, kFieldBase + 0), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
                    objectRecord.TileY =
                        std::clamp(ReadI32(payload, kFieldBase + 4), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
                    objectRecord.OffsetX       = ReadI32(payload, kFieldBase + 8);
                    objectRecord.OffsetY       = ReadI32(payload, kFieldBase + 12);
                    objectRecord.FrameInterval = std::max(1, ReadI32(payload, kFieldBase + 16));
                    objectRecord.ShowMode      = ReadI32(payload, kFieldBase + 20);
                    objectRecord.FrameCount    = 0;

                    ++outParsedCount;
                    outMapObjects.push_back(std::move(objectRecord));
                    break;
                }

                case kLegacyMapObjCover:
                {
                    constexpr size_t kRecordBytes = kLegacyMapPathBytes + kLegacyMapTitleBytes + sizeof(int32_t) * 7;
                    if (!ReadFixedBlock(bytes, ioCursor, kRecordBytes, "MAP_COVER payload", payload, outWarnings))
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
                    objectRecord.TileX = std::clamp(ReadI32(payload, kFieldBase + 0), 0, static_cast<int32_t>(descriptor.WidthInTiles) - 1);
                    objectRecord.TileY =
                        std::clamp(ReadI32(payload, kFieldBase + 4), 0, static_cast<int32_t>(descriptor.HeightInTiles) - 1);
                    objectRecord.BaseWidth     = std::max(1, ReadI32(payload, kFieldBase + 8));
                    objectRecord.BaseHeight    = std::max(1, ReadI32(payload, kFieldBase + 12));
                    objectRecord.OffsetX       = ReadI32(payload, kFieldBase + 16);
                    objectRecord.OffsetY       = ReadI32(payload, kFieldBase + 20);
                    objectRecord.FrameInterval = std::max(1, ReadI32(payload, kFieldBase + 24));
                    objectRecord.FrameCount    = 0;
                    objectRecord.ShowMode      = 0;

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

} // namespace LXMap
