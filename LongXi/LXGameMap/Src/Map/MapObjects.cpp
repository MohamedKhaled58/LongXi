#include "Map/MapObjects.h"

#include <algorithm>

namespace LongXi
{

void MapObjects::Clear()
{
    m_Objects.clear();
    m_NextInsertionOrder = 1;
}

void MapObjects::AddObject(MapObjectRecord objectRecord)
{
    if (objectRecord.InsertionOrder == 0)
    {
        objectRecord.InsertionOrder = m_NextInsertionOrder;
    }

    ++m_NextInsertionOrder;
    m_Objects.push_back(std::move(objectRecord));
}

const std::vector<MapObjectRecord>& MapObjects::GetObjects() const
{
    return m_Objects;
}

void MapObjects::CollectVisible(const VisibleTileWindow& window, std::vector<const MapObjectRecord*>& outVisibleObjects) const
{
    outVisibleObjects.clear();
    if (window.IsEmpty())
    {
        return;
    }

    outVisibleObjects.reserve(m_Objects.size());
    for (const MapObjectRecord& objectRecord : m_Objects)
    {
        if (objectRecord.TileX < window.StartTileX || objectRecord.TileX > window.EndTileX || objectRecord.TileY < window.StartTileY ||
            objectRecord.TileY > window.EndTileY)
        {
            continue;
        }

        outVisibleObjects.push_back(&objectRecord);
    }

    std::ranges::sort(outVisibleObjects,
                      [](const MapObjectRecord* lhs, const MapObjectRecord* rhs)
                      {
                          if (lhs->RenderLayer != rhs->RenderLayer)
                          {
                              return static_cast<uint8_t>(lhs->RenderLayer) < static_cast<uint8_t>(rhs->RenderLayer);
                          }

                          const int32_t lhsDepth = lhs->GetDepthKey();
                          const int32_t rhsDepth = rhs->GetDepthKey();
                          if (lhsDepth != rhsDepth)
                          {
                              return lhsDepth < rhsDepth;
                          }

                          return lhs->InsertionOrder < rhs->InsertionOrder;
                      });
}

} // namespace LongXi