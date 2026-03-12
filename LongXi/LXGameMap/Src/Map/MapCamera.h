#pragma once

#include "Map/MapTypes.h"

namespace LongXi
{

class MapCamera
{
public:
    void SetMapDescriptor(const MapDescriptor& descriptor);

    void OnResize(uint32_t viewportWidth, uint32_t viewportHeight);

    void SetViewCenter(float worldX, float worldY);
    void SetZoom(float zoom);
    void SetCullMargins(uint32_t marginTilesX, uint32_t marginTilesY);

    float GetViewCenterWorldX() const;
    float GetViewCenterWorldY() const;
    float GetZoom() const;

    bool TileToWorld(int32_t tileX, int32_t tileY, int16_t height, float& outWorldX, float& outWorldY, float& outWorldZ) const;
    bool WorldToTile(float worldX, float worldY, int32_t& outTileX, int32_t& outTileY) const;
    void WorldToScreen(float worldX, float worldY, float worldZ, float& outScreenX, float& outScreenY) const;

    VisibleTileWindow ComputeVisibleWindow() const;

private:
    int32_t ClampTileX(int32_t value) const;
    int32_t ClampTileY(int32_t value) const;

private:
    uint32_t m_MapWidth   = 0;
    uint32_t m_MapHeight  = 0;
    uint32_t m_CellWidth  = 64;
    uint32_t m_CellHeight = 32;
    int32_t  m_OriginX    = 0;
    int32_t  m_OriginY    = 0;

    uint32_t m_ViewportWidth  = 1;
    uint32_t m_ViewportHeight = 1;

    float m_ViewCenterWorldX = 0.0f;
    float m_ViewCenterWorldY = 0.0f;
    float m_Zoom             = 1.0f;

    uint32_t m_CullMarginTilesX = 2;
    uint32_t m_CullMarginTilesY = 2;
};

} // namespace LongXi