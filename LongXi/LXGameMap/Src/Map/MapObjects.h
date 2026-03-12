#pragma once

#include <vector>

#include "Map/MapTypes.h"

namespace LongXi
{

class MapObjects
{
public:
    void Clear();

    void                                AddObject(MapObjectRecord objectRecord);
    const std::vector<MapObjectRecord>& GetObjects() const;

    void CollectVisible(const VisibleTileWindow& window, std::vector<const MapObjectRecord*>& outVisibleObjects) const;

private:
    std::vector<MapObjectRecord> m_Objects;
    uint64_t                     m_NextInsertionOrder = 1;
};

} // namespace LongXi