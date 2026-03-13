#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace LongXi
{

class CVirtualFileSystem;

// Descriptor for a single terrain-part entry parsed from a .scene binary block.
struct TerrainPartDescriptor
{
    std::string AniPath;
    std::string AniTag;
    int32_t     SpriteOffsetX = 0;
    int32_t     SpriteOffsetY = 0;
    int32_t     FrameInterval = 0;
    int32_t     BaseWidth     = 1;
    int32_t     BaseHeight    = 1;
    int32_t     ShowType      = 0;
    int32_t     SceneOffsetX  = 0;
    int32_t     SceneOffsetY  = 0;
    int32_t     HeightOffset  = 0;
};

// Parse a .ani INI-style animation descriptor file.
// Populates outFramesByPuzzle: puzzle-index → ordered list of frame texture paths.
// Returns false when the file is missing or unreadable.
bool ParseAni(const std::string&                                      aniPath,
              CVirtualFileSystem&                                     vfs,
              std::unordered_map<uint16_t, std::vector<std::string>>& outFramesByPuzzle,
              std::vector<std::string>&                               outWarnings);

// Parse a .scene binary block into TerrainPartDescriptor records.
// Returns false when bytes is empty or no valid records can be decoded.
bool ParseTerrainPartDescriptors(const std::vector<uint8_t>& bytes, std::vector<TerrainPartDescriptor>& outDescriptors);

} // namespace LongXi
