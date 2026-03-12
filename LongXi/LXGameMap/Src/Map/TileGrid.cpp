#include "Map/TileGrid.h"

#include <algorithm>
#include <utility>

namespace LongXi
{

bool TileGrid::Initialize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }

    m_Width = width;
    m_Height = height;
    m_Tiles.clear();
    m_Tiles.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    m_RowChecksums.assign(height, 0u);
    m_IntegrityValid = true;
    return true;
}

void TileGrid::Clear()
{
    m_Width = 0;
    m_Height = 0;
    m_Tiles.clear();
    m_RowChecksums.clear();
    m_IntegrityValid = true;
}

bool TileGrid::IsValid() const
{
    return m_Width > 0 && m_Height > 0 && m_Tiles.size() == static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) && m_IntegrityValid;
}

uint32_t TileGrid::GetWidth() const
{
    return m_Width;
}

uint32_t TileGrid::GetHeight() const
{
    return m_Height;
}

TileRecord* TileGrid::GetTile(int32_t x, int32_t y)
{
    if (!IsInBounds(x, y))
    {
        return nullptr;
    }

    return &m_Tiles[GetIndex(x, y)];
}

const TileRecord* TileGrid::GetTile(int32_t x, int32_t y) const
{
    if (!IsInBounds(x, y))
    {
        return nullptr;
    }

    return &m_Tiles[GetIndex(x, y)];
}

bool TileGrid::SetTile(const TileRecord& tile)
{
    if (!IsInBounds(tile.TileX, tile.TileY))
    {
        return false;
    }

    m_Tiles[GetIndex(tile.TileX, tile.TileY)] = tile;
    return true;
}

void TileGrid::ComputeRowChecksums()
{
    if (m_Width == 0 || m_Height == 0)
    {
        m_RowChecksums.clear();
        return;
    }

    m_RowChecksums.assign(m_Height, 0u);

    for (uint32_t y = 0; y < m_Height; ++y)
    {
        uint32_t rowChecksum = 0;
        for (uint32_t x = 0; x < m_Width; ++x)
        {
            const TileRecord& tile = m_Tiles[GetIndex(static_cast<int32_t>(x), static_cast<int32_t>(y))];
            const int32_t left = static_cast<int32_t>(tile.MaskId) * (static_cast<int32_t>(tile.TerrainId) + static_cast<int32_t>(y) + 1);
            const int32_t right = (static_cast<int32_t>(tile.Height) + 2) * (static_cast<int32_t>(x) + 1 + static_cast<int32_t>(tile.TerrainId));
            rowChecksum += static_cast<uint32_t>(left + right);
        }

        m_RowChecksums[y] = rowChecksum;
    }
}

bool TileGrid::ValidateRowChecksums(const std::vector<uint32_t>& expectedChecksums)
{
    ComputeRowChecksums();

    if (expectedChecksums.empty())
    {
        m_IntegrityValid = true;
        return true;
    }

    if (expectedChecksums.size() != m_RowChecksums.size())
    {
        m_IntegrityValid = false;
        return false;
    }

    for (size_t i = 0; i < expectedChecksums.size(); ++i)
    {
        if (expectedChecksums[i] != m_RowChecksums[i])
        {
            m_IntegrityValid = false;
            return false;
        }
    }

    m_IntegrityValid = true;
    return true;
}

void TileGrid::EnumerateVisible(const VisibleTileWindow& window, std::vector<const TileRecord*>& outTiles) const
{
    outTiles.clear();
    if (!IsValid() || window.IsEmpty())
    {
        return;
    }

    const int32_t minX = std::max<int32_t>(0, window.StartTileX);
    const int32_t minY = std::max<int32_t>(0, window.StartTileY);
    const int32_t maxX = std::min<int32_t>(static_cast<int32_t>(m_Width) - 1, window.EndTileX);
    const int32_t maxY = std::min<int32_t>(static_cast<int32_t>(m_Height) - 1, window.EndTileY);

    if (minX > maxX || minY > maxY)
    {
        return;
    }

    const int32_t minDepth = minX + minY;
    const int32_t maxDepth = maxX + maxY;

    outTiles.reserve(static_cast<size_t>((maxX - minX + 1) * (maxY - minY + 1)));

    for (int32_t depth = minDepth; depth <= maxDepth; ++depth)
    {
        const int32_t startX = std::max(minX, depth - maxY);
        const int32_t endX = std::min(maxX, depth - minY);
        for (int32_t x = startX; x <= endX; ++x)
        {
            const int32_t y = depth - x;
            if (!IsInBounds(x, y))
            {
                continue;
            }

            outTiles.push_back(&m_Tiles[GetIndex(x, y)]);
        }
    }
}

bool TileGrid::IsInBounds(int32_t x, int32_t y) const
{
    return x >= 0 && y >= 0 && std::cmp_less(x, m_Width) && std::cmp_less(y, m_Height);
}

size_t TileGrid::GetIndex(int32_t x, int32_t y) const
{
    return static_cast<size_t>(y) * static_cast<size_t>(m_Width) + static_cast<size_t>(x);
}

} // namespace LongXi
