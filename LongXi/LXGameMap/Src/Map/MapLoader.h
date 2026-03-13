#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map/MapTypes.h"

namespace LongXi
{

class CVirtualFileSystem;
class Texture;
class TextureManager;
class TileGrid;

class MapLoader
{
public:
    static bool LoadMap(const std::string&                                      mapPath,
                        CVirtualFileSystem&                                     vfs,
                        TextureManager&                                         textureManager,
                        MapDescriptor&                                          outDescriptor,
                        TileGrid&                                               outTileGrid,
                        std::vector<MapObjectRecord>&                           outMapObjects,
                        std::vector<MapAnimationState>&                         outAnimations,
                        std::unordered_map<uint16_t, std::shared_ptr<Texture>>& outTextureRefs,
                        std::vector<std::string>&                               outWarnings);

private:
    static bool ParseDmap(const std::string&            mapPath,
                          CVirtualFileSystem&           vfs,
                          MapDescriptor&                outDescriptor,
                          TileGrid&                     outTileGrid,
                          std::vector<MapObjectRecord>& outMapObjects,
                          std::vector<std::string>&     outWarnings);

    static bool ResolveMapPath(const std::string&        requestedMapPath,
                               CVirtualFileSystem&       vfs,
                               std::string&              outResolvedPath,
                               uint32_t&                 outMapId,
                               uint32_t&                 outMapFlags,
                               std::vector<std::string>& outWarnings);

    static bool ParseGameMapDat(CVirtualFileSystem& vfs, std::vector<MapCatalogEntry>& outEntries, std::vector<std::string>& outWarnings);

    static bool        LooksLikeNumericMapId(const std::string& value);
    static std::string Trim(std::string value);
    static std::string NormalizeResourcePath(const std::string& value);
    static std::string JoinPath(const std::string& basePath, const std::string& relativePath);
    static std::string BaseFileNameWithoutExtension(const std::string& path);
    static std::string BuildMapAniPath(const std::string& mapPath);
    static bool        TryResolveTexturePath(const std::string&   framePath,
                                             const MapDescriptor& descriptor,
                                             CVirtualFileSystem&  vfs,
                                             std::string&         outResolvedPath);
    static bool        WorldToTile(const MapDescriptor& descriptor, int32_t worldX, int32_t worldY, int32_t& outTileX, int32_t& outTileY);
    static bool        ParsePassageBlock(const std::vector<uint8_t>&    bytes,
                                         size_t&                        ioCursor,
                                         std::vector<MapPassageRecord>& outPassages,
                                         std::vector<std::string>&      outWarnings);
    static bool        ParseInteractiveObjectBlock(const std::vector<uint8_t>&   bytes,
                                                   size_t&                       ioCursor,
                                                   const MapDescriptor&          descriptor,
                                                   std::vector<MapObjectRecord>& outMapObjects,
                                                   uint32_t&                     ioObjectId,
                                                   uint32_t&                     outParsedCount,
                                                   uint32_t&                     outUnknownCount,
                                                   bool&                         outAborted,
                                                   std::vector<std::string>&     outWarnings);
    static bool        ParseSceneLayerBlock(const std::vector<uint8_t>&   bytes,
                                            size_t&                       ioCursor,
                                            const MapDescriptor&          descriptor,
                                            std::vector<MapObjectRecord>& outMapObjects,
                                            uint32_t&                     ioObjectId,
                                            uint32_t&                     outParsedCount,
                                            uint32_t&                     outUnknownCount,
                                            bool&                         outAborted,
                                            std::vector<std::string>&     outWarnings);
    static bool        ReadFixedBlock(const std::vector<uint8_t>& bytes,
                                      size_t&                     ioCursor,
                                      size_t                      blockBytes,
                                      const char*                 blockName,
                                      std::vector<uint8_t>&       outBlock,
                                      std::vector<std::string>&   outWarnings);
    static uint16_t    ReadU16(const std::vector<uint8_t>& bytes, size_t offset);
    static int16_t     ReadI16(const std::vector<uint8_t>& bytes, size_t offset);
    static uint32_t    ReadU32(const std::vector<uint8_t>& bytes, size_t offset);
    static int32_t     ReadI32(const std::vector<uint8_t>& bytes, size_t offset);
    static std::string ReadFixedCString(const std::vector<uint8_t>& bytes, size_t offset, size_t length);

    static std::string ReplaceExtension(const std::string& path, const std::string& extension);
    static void        AddWarning(std::vector<std::string>& warnings, const std::string& message);
};

} // namespace LongXi
