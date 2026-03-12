#pragma once

#include <cstdint>
#include <vector>

#include "Map/MapTypes.h"

namespace LongXi
{

class TileGrid
{
public:
    bool Initialize(uint32_t width, uint32_t height);
    void Clear();

    bool IsValid() const;

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

    TileRecord*       GetTile(int32_t x, int32_t y);
    const TileRecord* GetTile(int32_t x, int32_t y) const;

    bool SetTile(const TileRecord& tile);

    void ComputeRowChecksums();
    bool ValidateRowChecksums(const std::vector<uint32_t>& expectedChecksums);

    void EnumerateVisible(const VisibleTileWindow& window, std::vector<const TileRecord*>& outTiles) const;

private:
    bool   IsInBounds(int32_t x, int32_t y) const;
    size_t GetIndex(int32_t x, int32_t y) const;

private:
    uint32_t                m_Width  = 0;
    uint32_t                m_Height = 0;
    std::vector<TileRecord> m_Tiles;
    std::vector<uint32_t>   m_RowChecksums;
    bool                    m_IntegrityValid = true;
};

} // namespace LongXi