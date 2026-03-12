#include "Map/MapCamera.h"

#include <algorithm>
#include <cmath>

namespace LongXi
{

void MapCamera::SetMapDescriptor(const MapDescriptor& descriptor)
{
    m_MapWidth = descriptor.WidthInTiles;
    m_MapHeight = descriptor.HeightInTiles;
    m_CellWidth = descriptor.CellWidth;
    m_CellHeight = descriptor.CellHeight;
    m_OriginX = descriptor.OriginX;
    m_OriginY = descriptor.OriginY;

    const float halfW = static_cast<float>(m_MapWidth) * 0.5f;
    const float halfH = static_cast<float>(m_MapHeight) * 0.5f;
    m_ViewCenterWorldX = static_cast<float>(m_CellWidth) * (halfW - halfH) * 0.5f + static_cast<float>(m_OriginX);
    m_ViewCenterWorldY = static_cast<float>(m_CellHeight) * (halfW + halfH) * 0.5f + static_cast<float>(m_OriginY);
}

void MapCamera::OnResize(uint32_t viewportWidth, uint32_t viewportHeight)
{
    m_ViewportWidth = std::max<uint32_t>(1, viewportWidth);
    m_ViewportHeight = std::max<uint32_t>(1, viewportHeight);
}

void MapCamera::SetViewCenter(float worldX, float worldY)
{
    m_ViewCenterWorldX = worldX;
    m_ViewCenterWorldY = worldY;
}

void MapCamera::SetZoom(float zoom)
{
    m_Zoom = std::max(zoom, 0.1f);
}

void MapCamera::SetCullMargins(uint32_t marginTilesX, uint32_t marginTilesY)
{
    m_CullMarginTilesX = marginTilesX;
    m_CullMarginTilesY = marginTilesY;
}

float MapCamera::GetViewCenterWorldX() const
{
    return m_ViewCenterWorldX;
}

float MapCamera::GetViewCenterWorldY() const
{
    return m_ViewCenterWorldY;
}

float MapCamera::GetZoom() const
{
    return m_Zoom;
}

bool MapCamera::TileToWorld(int32_t tileX, int32_t tileY, int16_t height, float& outWorldX, float& outWorldY, float& outWorldZ) const
{
    if (tileX < 0 || tileY < 0 || static_cast<uint32_t>(tileX) >= m_MapWidth || static_cast<uint32_t>(tileY) >= m_MapHeight)
    {
        return false;
    }

    const float cellWidth = static_cast<float>(m_CellWidth);
    const float cellHeight = static_cast<float>(m_CellHeight);

    outWorldX = cellWidth * static_cast<float>(tileX - tileY) * 0.5f + static_cast<float>(m_OriginX);
    outWorldY = cellHeight * static_cast<float>(tileX + tileY) * 0.5f + static_cast<float>(m_OriginY);
    outWorldZ = static_cast<float>(height);
    return true;
}

bool MapCamera::WorldToTile(float worldX, float worldY, int32_t& outTileX, int32_t& outTileY) const
{
    if (m_CellWidth == 0 || m_CellHeight == 0 || m_MapWidth == 0 || m_MapHeight == 0)
    {
        return false;
    }

    const double normalizedX = static_cast<double>(worldX - static_cast<float>(m_OriginX));
    const double normalizedY = static_cast<double>(worldY - static_cast<float>(m_OriginY));

    const double halfCellWidth = static_cast<double>(m_CellWidth) * 0.5;
    const double halfCellHeight = static_cast<double>(m_CellHeight) * 0.5;

    if (halfCellWidth <= 0.0 || halfCellHeight <= 0.0)
    {
        return false;
    }

    const double tileXF = (normalizedX / halfCellWidth + normalizedY / halfCellHeight) * 0.5;
    const double tileYF = (normalizedY / halfCellHeight - normalizedX / halfCellWidth) * 0.5;

    outTileX = ClampTileX(static_cast<int32_t>(std::floor(tileXF)));
    outTileY = ClampTileY(static_cast<int32_t>(std::floor(tileYF)));
    return true;
}

void MapCamera::WorldToScreen(float worldX, float worldY, float worldZ, float& outScreenX, float& outScreenY) const
{
    outScreenX = (worldX - m_ViewCenterWorldX) * m_Zoom + static_cast<float>(m_ViewportWidth) * 0.5f;
    outScreenY = (worldY - worldZ - m_ViewCenterWorldY) * m_Zoom + static_cast<float>(m_ViewportHeight) * 0.5f;
}

VisibleTileWindow MapCamera::ComputeVisibleWindow() const
{
    VisibleTileWindow window;
    if (m_MapWidth == 0 || m_MapHeight == 0)
    {
        window.StartTileX = 1;
        window.EndTileX = 0;
        return window;
    }

    const float halfWidthWorld = static_cast<float>(m_ViewportWidth) * 0.5f / m_Zoom;
    const float halfHeightWorld = static_cast<float>(m_ViewportHeight) * 0.5f / m_Zoom;

    const float marginX = static_cast<float>(m_CullMarginTilesX * m_CellWidth);
    const float marginY = static_cast<float>(m_CullMarginTilesY * m_CellHeight);

    const float left = m_ViewCenterWorldX - halfWidthWorld - marginX;
    const float right = m_ViewCenterWorldX + halfWidthWorld + marginX;
    const float top = m_ViewCenterWorldY - halfHeightWorld - marginY;
    const float bottom = m_ViewCenterWorldY + halfHeightWorld + marginY;

    int32_t tx0 = 0;
    int32_t ty0 = 0;
    int32_t tx1 = 0;
    int32_t ty1 = 0;
    int32_t tx2 = 0;
    int32_t ty2 = 0;
    int32_t tx3 = 0;
    int32_t ty3 = 0;

    WorldToTile(left, top, tx0, ty0);
    WorldToTile(right, top, tx1, ty1);
    WorldToTile(left, bottom, tx2, ty2);
    WorldToTile(right, bottom, tx3, ty3);

    const int32_t minX = std::min(std::min(tx0, tx1), std::min(tx2, tx3));
    const int32_t maxX = std::max(std::max(tx0, tx1), std::max(tx2, tx3));
    const int32_t minY = std::min(std::min(ty0, ty1), std::min(ty2, ty3));
    const int32_t maxY = std::max(std::max(ty0, ty1), std::max(ty2, ty3));

    window.StartTileX = ClampTileX(minX);
    window.EndTileX = ClampTileX(maxX);
    window.StartTileY = ClampTileY(minY);
    window.EndTileY = ClampTileY(maxY);
    return window;
}

int32_t MapCamera::ClampTileX(int32_t value) const
{
    if (m_MapWidth == 0)
    {
        return 0;
    }

    return std::clamp(value, 0, static_cast<int32_t>(m_MapWidth) - 1);
}

int32_t MapCamera::ClampTileY(int32_t value) const
{
    if (m_MapHeight == 0)
    {
        return 0;
    }

    return std::clamp(value, 0, static_cast<int32_t>(m_MapHeight) - 1);
}

} // namespace LongXi