#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Map/MapTypes.h"

namespace LXMap
{

class CVirtualFileSystem;
class Texture;
class TextureManager;
class TileGrid;
struct MapAnimationState;

// Parse a .pul puzzle texture-atlas file.
// Populates inOutDescriptor with puzzle grid dimensions, indices, and ANI path.
// Assigns TextureId on each tile in inOutTileGrid via the puzzle index grid.
// Loads puzzle frame textures into outTextureRefs and creates animation states
// in outAnimations for multi-frame entries.
// Returns false when no valid puzzle file is found or the format is unsupported.
bool ParsePuzzle(const std::string&                                      mapPath,
                 CVirtualFileSystem&                                     vfs,
                 TextureManager&                                         textureManager,
                 MapDescriptor&                                          inOutDescriptor,
                 TileGrid&                                               inOutTileGrid,
                 std::vector<MapAnimationState>&                         outAnimations,
                 std::unordered_map<uint16_t, std::shared_ptr<Texture>>& outTextureRefs,
                 std::vector<std::string>&                               outWarnings);

} // namespace LXMap
