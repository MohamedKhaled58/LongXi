#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Map/MapTypes.h"
#include "Map/TileGrid.h"

namespace LXMap
{

// Parse DMAP binary file format: file header, tile grid, and passage records.
// Reads from a pre-loaded byte buffer. On success outDescriptor (header fields and passages)
// and outTileGrid are populated, and outCursor is set to the position in bytes immediately
// after the passage block — ready for DmapObjectParser to consume.
bool ParseDmap(const std::string&          mapPath,
               const std::vector<uint8_t>& bytes,
               MapDescriptor&              outDescriptor,
               TileGrid&                   outTileGrid,
               size_t&                     outCursor,
               std::vector<std::string>&   outWarnings);

// Parse the passage block at the current cursor position in bytes.
// Called internally by ParseDmap; exposed for testing and diagnostic use.
bool ParsePassageBlock(const std::vector<uint8_t>&    bytes,
                       size_t&                        ioCursor,
                       std::vector<MapPassageRecord>& outPassages,
                       std::vector<std::string>&      outWarnings);

} // namespace LXMap
