#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Map/MapTypes.h"

namespace LongXi
{

// Parse the interactive object block at the current cursor position in the DMAP byte buffer.
// Populates outMapObjects with MAP_COVER, MAP_SCENE, MAP_TERRAIN, MAP_PUZZLE, MAP_SOUND, and
// 3D-effect records. ioObjectId is incremented for each parsed record.
// outAborted is set true when parsing stops early due to unsupported or corrupt data.
// Returns false only on a hard failure; partial results with outAborted=true are valid.
bool ParseInteractiveObjectBlock(const std::vector<uint8_t>&   bytes,
                                 size_t&                       ioCursor,
                                 const MapDescriptor&          descriptor,
                                 std::vector<MapObjectRecord>& outMapObjects,
                                 uint32_t&                     ioObjectId,
                                 uint32_t&                     outParsedCount,
                                 uint32_t&                     outUnknownCount,
                                 bool&                         outAborted,
                                 std::vector<std::string>&     outWarnings);

// Parse the scene-layer (extra layer) block at the current cursor position in the DMAP byte buffer.
// Appends MAP_SCENE and MAP_COVER records from parallax/sky layers into outMapObjects.
// Returns false only on a hard failure; partial results with outAborted=true are valid.
bool ParseSceneLayerBlock(const std::vector<uint8_t>&   bytes,
                          size_t&                       ioCursor,
                          const MapDescriptor&          descriptor,
                          std::vector<MapObjectRecord>& outMapObjects,
                          uint32_t&                     ioObjectId,
                          uint32_t&                     outParsedCount,
                          uint32_t&                     outUnknownCount,
                          bool&                         outAborted,
                          std::vector<std::string>&     outWarnings);

} // namespace LongXi
