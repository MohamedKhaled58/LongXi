#include "Map/MapObjects.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>

#include "Core/Logging/LogMacros.h"

namespace LXMap
{

namespace
{

constexpr uint16_t kLegacyMapObjTerrainPart = 2;
constexpr uint16_t kLegacyMapObjCover       = 4;

struct OrderingFootprint
{
    int32_t Width  = 1;
    int32_t Height = 1;
};

OrderingFootprint GetOrderingFootprint(const MapObjectRecord& objectRecord)
{
    if (objectRecord.ObjectType == kLegacyMapObjTerrainPart || objectRecord.ObjectType == kLegacyMapObjCover)
    {
        return {std::max(1, objectRecord.BaseWidth), std::max(1, objectRecord.BaseHeight)};
    }

    return {};
}

bool IsTerrainPartObject(const MapObjectRecord* objectRecord)
{
    return objectRecord != nullptr && objectRecord->ObjectType == kLegacyMapObjTerrainPart;
}

int32_t GetLegacyVisibilityPaddingTiles(const MapObjectRecord& objectRecord)
{
    if (objectRecord.ObjectType == kLegacyMapObjCover)
    {
        return 12;
    }

    if (objectRecord.RenderLayer == MapLayer::Interactive)
    {
        return 8;
    }

    return 2;
}

bool IsVisibleInWindow(const MapObjectRecord& objectRecord, const VisibleTileWindow& window)
{
    const OrderingFootprint footprint  = GetOrderingFootprint(objectRecord);
    const int32_t           minTileX   = objectRecord.TileX - footprint.Width + 1;
    const int32_t           minTileY   = objectRecord.TileY - footprint.Height + 1;
    const int32_t           padding    = GetLegacyVisibilityPaddingTiles(objectRecord);
    const int32_t           startTileX = window.StartTileX - padding;
    const int32_t           startTileY = window.StartTileY - padding;
    const int32_t           endTileX   = window.EndTileX + padding;
    const int32_t           endTileY   = window.EndTileY + padding;

    if (objectRecord.TileX < startTileX || minTileX > endTileX)
    {
        return false;
    }

    if (objectRecord.TileY < startTileY || minTileY > endTileY)
    {
        return false;
    }

    return true;
}

bool IsInTerrainPartBase(const MapObjectRecord& terrainPart, const MapObjectRecord& objectRecord)
{
    if (!IsTerrainPartObject(&terrainPart))
    {
        return false;
    }

    const OrderingFootprint footprint = GetOrderingFootprint(terrainPart);
    const int32_t           minTileX  = terrainPart.TileX - footprint.Width + 1;
    const int32_t           minTileY  = terrainPart.TileY - footprint.Height + 1;

    return objectRecord.TileX <= terrainPart.TileX && objectRecord.TileY <= terrainPart.TileY && objectRecord.TileX >= minTileX &&
           objectRecord.TileY >= minTileY;
}

void InsertLegacyDepthOrdered(const MapObjectRecord* objectRecord, std::vector<const MapObjectRecord*>& orderedObjects)
{
    if (objectRecord == nullptr)
    {
        return;
    }

    const OrderingFootprint newFootprint = GetOrderingFootprint(*objectRecord);
    const int32_t           newObjectX   = objectRecord->TileX;
    const int32_t           newObjectY   = objectRecord->TileY;
    const int32_t           newObjectSum = newObjectX + newObjectY;

    int    overlayAfterIndex = -1;
    size_t compareIndex      = 0;
    for (; compareIndex < orderedObjects.size(); ++compareIndex)
    {
        const MapObjectRecord* oldObject = orderedObjects[compareIndex];
        if (oldObject == nullptr)
        {
            continue;
        }

        const OrderingFootprint oldFootprint = GetOrderingFootprint(*oldObject);
        const int32_t           oldObjectX   = oldObject->TileX;
        const int32_t           oldObjectY   = oldObject->TileY;
        const int32_t           oldObjectSum = oldObjectX + oldObjectY;

        const bool noXOverlay =
            (newObjectX - newFootprint.Width + 1) - oldObjectX > 0 || (oldObjectX - oldFootprint.Width + 1) - newObjectX > 0;
        if (noXOverlay)
        {
            const bool noYOverlay =
                (newObjectY - newFootprint.Height + 1) - oldObjectY > 0 || (oldObjectY - oldFootprint.Height + 1) - newObjectY > 0;
            if (!noYOverlay)
            {
                if (newObjectX < oldObjectX)
                {
                    int insertIndex = static_cast<int>(compareIndex);
                    for (int probeIndex = static_cast<int>(compareIndex) - 1; probeIndex >= 0; --probeIndex)
                    {
                        const MapObjectRecord* probeObject = orderedObjects[static_cast<size_t>(probeIndex)];
                        if (probeObject == nullptr)
                        {
                            continue;
                        }

                        const int32_t probeObjectSum = probeObject->TileX + probeObject->TileY;
                        if ((oldObjectSum == probeObjectSum || newObjectSum < probeObjectSum) && probeIndex > overlayAfterIndex)
                        {
                            insertIndex = probeIndex;
                        }
                        else
                        {
                            break;
                        }
                    }

                    orderedObjects.insert(orderedObjects.begin() + insertIndex, objectRecord);
                    return;
                }

                overlayAfterIndex = static_cast<int>(compareIndex);
                for (int probeIndex = static_cast<int>(compareIndex) + 1; probeIndex < static_cast<int>(orderedObjects.size());
                     ++probeIndex)
                {
                    const MapObjectRecord* probeObject = orderedObjects[static_cast<size_t>(probeIndex)];
                    if (probeObject == nullptr)
                    {
                        continue;
                    }

                    const int32_t probeObjectSum = probeObject->TileX + probeObject->TileY;
                    if (oldObjectSum == probeObjectSum || newObjectSum > probeObjectSum)
                    {
                        overlayAfterIndex = probeIndex;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            if (newObjectY < oldObjectY)
            {
                int insertIndex = static_cast<int>(compareIndex);
                for (int probeIndex = static_cast<int>(compareIndex) - 1; probeIndex >= 0; --probeIndex)
                {
                    const MapObjectRecord* probeObject = orderedObjects[static_cast<size_t>(probeIndex)];
                    if (probeObject == nullptr)
                    {
                        continue;
                    }

                    const int32_t probeObjectSum = probeObject->TileX + probeObject->TileY;
                    if ((oldObjectSum == probeObjectSum || newObjectSum < probeObjectSum) && probeIndex > overlayAfterIndex)
                    {
                        insertIndex = probeIndex;
                    }
                    else
                    {
                        break;
                    }
                }

                orderedObjects.insert(orderedObjects.begin() + insertIndex, objectRecord);
                return;
            }

            overlayAfterIndex = static_cast<int>(compareIndex);
            for (int probeIndex = static_cast<int>(compareIndex) + 1; probeIndex < static_cast<int>(orderedObjects.size()); ++probeIndex)
            {
                const MapObjectRecord* probeObject = orderedObjects[static_cast<size_t>(probeIndex)];
                if (probeObject == nullptr)
                {
                    continue;
                }

                const int32_t probeObjectSum = probeObject->TileX + probeObject->TileY;
                if (oldObjectSum == probeObjectSum || newObjectSum > probeObjectSum)
                {
                    overlayAfterIndex = probeIndex;
                }
                else
                {
                    break;
                }
            }
        }
    }

    if (overlayAfterIndex != -1)
    {
        if (overlayAfterIndex >= static_cast<int>(orderedObjects.size()) - 1)
        {
            orderedObjects.push_back(objectRecord);
        }
        else
        {
            orderedObjects.insert(orderedObjects.begin() + overlayAfterIndex + 1, objectRecord);
        }
        return;
    }

    if (compareIndex >= orderedObjects.size())
    {
        for (size_t insertIndex = 0; insertIndex < orderedObjects.size(); ++insertIndex)
        {
            const MapObjectRecord* oldObject = orderedObjects[insertIndex];
            if (oldObject == nullptr)
            {
                continue;
            }

            if (newObjectSum < oldObject->TileX + oldObject->TileY)
            {
                orderedObjects.insert(orderedObjects.begin() + static_cast<std::ptrdiff_t>(insertIndex), objectRecord);
                return;
            }
        }

        orderedObjects.push_back(objectRecord);
    }
}

void InsertTerrainPartChild(const MapObjectRecord* objectRecord, std::vector<const MapObjectRecord*>& orderedChildren)
{
    if (objectRecord == nullptr)
    {
        return;
    }

    const OrderingFootprint newFootprint = GetOrderingFootprint(*objectRecord);
    const int32_t           newObjectX   = objectRecord->TileX;
    const int32_t           newObjectY   = objectRecord->TileY;
    const int32_t           newObjectSum = newObjectX + newObjectY;

    size_t compareIndex = 0;
    for (; compareIndex < orderedChildren.size(); ++compareIndex)
    {
        const MapObjectRecord* oldObject = orderedChildren[compareIndex];
        if (oldObject == nullptr)
        {
            continue;
        }

        const OrderingFootprint oldFootprint = GetOrderingFootprint(*oldObject);
        const int32_t           oldObjectX   = oldObject->TileX;
        const int32_t           oldObjectY   = oldObject->TileY;
        const int32_t           oldObjectSum = oldObjectX + oldObjectY;

        const bool noXOverlay =
            (newObjectX - newFootprint.Width + 1) - oldObjectX > 0 || (oldObjectX - oldFootprint.Width + 1) - newObjectX > 0;
        if (noXOverlay)
        {
            const int32_t newBottom = newObjectY - newFootprint.Height + 1;
            const int32_t oldBottom = oldObjectY - oldFootprint.Height + 1;
            if (newBottom > oldObjectY || oldBottom > newObjectY)
            {
                if (newObjectSum < oldObjectSum)
                {
                    orderedChildren.insert(orderedChildren.begin() + static_cast<std::ptrdiff_t>(compareIndex), objectRecord);
                    return;
                }
            }
            else if (newObjectX < oldObjectX)
            {
                orderedChildren.insert(orderedChildren.begin() + static_cast<std::ptrdiff_t>(compareIndex), objectRecord);
                return;
            }
        }
        else if (newObjectY < oldObjectY)
        {
            orderedChildren.insert(orderedChildren.begin() + static_cast<std::ptrdiff_t>(compareIndex), objectRecord);
            return;
        }
    }

    orderedChildren.push_back(objectRecord);
}

void AppendFlattenedObjects(const MapObjectRecord*                                                                 objectRecord,
                            const std::unordered_map<const MapObjectRecord*, std::vector<const MapObjectRecord*>>& attachedObjects,
                            std::vector<const MapObjectRecord*>&                                                   outVisibleObjects)
{
    if (objectRecord == nullptr)
    {
        return;
    }

    outVisibleObjects.push_back(objectRecord);

    const auto attachedIt = attachedObjects.find(objectRecord);
    if (attachedIt == attachedObjects.end())
    {
        return;
    }

    for (const MapObjectRecord* attachedObject : attachedIt->second)
    {
        AppendFlattenedObjects(attachedObject, attachedObjects, outVisibleObjects);
    }
}

bool NonInteractiveSortPredicate(const MapObjectRecord* lhs, const MapObjectRecord* rhs)
{
    if (lhs == nullptr || rhs == nullptr)
    {
        return lhs != nullptr;
    }

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

    if (lhs->TileY != rhs->TileY)
    {
        return lhs->TileY < rhs->TileY;
    }

    if (lhs->TileX != rhs->TileX)
    {
        return lhs->TileX < rhs->TileX;
    }

    return lhs->InsertionOrder < rhs->InsertionOrder;
}

} // namespace

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

    std::vector<const MapObjectRecord*> terrainPartCandidates;
    std::vector<const MapObjectRecord*> otherInteractiveCandidates;
    std::vector<const MapObjectRecord*> nonInteractiveCandidates;
    terrainPartCandidates.reserve(m_Objects.size());
    otherInteractiveCandidates.reserve(m_Objects.size());
    nonInteractiveCandidates.reserve(m_Objects.size());

    for (const MapObjectRecord& objectRecord : m_Objects)
    {
        if (!IsVisibleInWindow(objectRecord, window))
        {
            continue;
        }

        if (objectRecord.RenderLayer == MapLayer::Interactive)
        {
            if (IsTerrainPartObject(&objectRecord))
            {
                terrainPartCandidates.push_back(&objectRecord);
            }
            else
            {
                otherInteractiveCandidates.push_back(&objectRecord);
            }
        }
        else
        {
            nonInteractiveCandidates.push_back(&objectRecord);
        }
    }

    std::ranges::sort(nonInteractiveCandidates, NonInteractiveSortPredicate);

    std::vector<const MapObjectRecord*>                                             orderedInteractiveObjects;
    std::unordered_map<const MapObjectRecord*, std::vector<const MapObjectRecord*>> attachedInteractiveObjects;
    orderedInteractiveObjects.reserve(terrainPartCandidates.size() + otherInteractiveCandidates.size());

    for (const MapObjectRecord* objectRecord : terrainPartCandidates)
    {
        InsertLegacyDepthOrdered(objectRecord, orderedInteractiveObjects);
    }

    for (auto candidateIt = otherInteractiveCandidates.rbegin(); candidateIt != otherInteractiveCandidates.rend(); ++candidateIt)
    {
        const MapObjectRecord* objectRecord          = *candidateIt;
        bool                   attachedToTerrainPart = false;
        for (const MapObjectRecord* orderedObject : orderedInteractiveObjects)
        {
            if (orderedObject == nullptr || !IsTerrainPartObject(orderedObject))
            {
                continue;
            }

            if (IsInTerrainPartBase(*orderedObject, *objectRecord))
            {
                InsertTerrainPartChild(objectRecord, attachedInteractiveObjects[orderedObject]);
                attachedToTerrainPart = true;
                break;
            }
        }

        if (!attachedToTerrainPart)
        {
            InsertLegacyDepthOrdered(objectRecord, orderedInteractiveObjects);
        }
    }

    outVisibleObjects.reserve(nonInteractiveCandidates.size() + terrainPartCandidates.size() + otherInteractiveCandidates.size());
    for (const MapObjectRecord* objectRecord : nonInteractiveCandidates)
    {
        if (objectRecord != nullptr && static_cast<uint8_t>(objectRecord->RenderLayer) < static_cast<uint8_t>(MapLayer::Interactive))
        {
            outVisibleObjects.push_back(objectRecord);
        }
    }

    for (const MapObjectRecord* objectRecord : orderedInteractiveObjects)
    {
        AppendFlattenedObjects(objectRecord, attachedInteractiveObjects, outVisibleObjects);
    }

    for (const MapObjectRecord* objectRecord : nonInteractiveCandidates)
    {
        if (objectRecord != nullptr && static_cast<uint8_t>(objectRecord->RenderLayer) > static_cast<uint8_t>(MapLayer::Interactive))
        {
            outVisibleObjects.push_back(objectRecord);
        }
    }
}

} // namespace LXMap
