#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

template <typename T> class ResourceCache
{
public:
    struct CacheEntry
    {
        uint64_t           id = 0;
        std::shared_ptr<T> resource;
    };

    ResourceCache()  = default;
    ~ResourceCache() = default;

    void SetMaxSize(size_t maxSize)
    {
        m_MaxSize = maxSize;
        while (m_LruList.size() > m_MaxSize)
        {
            EvictOne();
        }
    }

    std::shared_ptr<T> Get(uint64_t id)
    {
        auto it = m_LruMap.find(id);
        if (it == m_LruMap.end())
            return nullptr;

        // Move to front (most recently used)
        m_LruList.splice(m_LruList.begin(), m_LruList, it->second);
        return it->second->resource;
    }

    void Put(uint64_t id, std::shared_ptr<T> resource)
    {
        if (!resource)
        {
            LX_ENGINE_WARN("[ResourceCache] Rejecting null resource for ID {}", id);
            return;
        }

        auto it = m_LruMap.find(id);
        if (it != m_LruMap.end())
        {
            // Update existing entry
            it->second->resource = resource;
            m_LruList.splice(m_LruList.begin(), m_LruList, it->second);
        }
        else
        {
            // Insert new entry
            m_LruList.push_front({id, resource});
            m_LruMap[id] = m_LruList.begin();

            // Evict if over capacity
            while (m_LruList.size() > m_MaxSize)
            {
                EvictOne();
            }
        }
    }

    bool Contains(uint64_t id) const
    {
        return m_LruMap.find(id) != m_LruMap.end();
    }

    size_t Size() const
    {
        return m_LruList.size();
    }

    void Clear()
    {
        size_t count = m_LruList.size();
        m_LruList.clear();
        m_LruMap.clear();
        LX_ENGINE_INFO("[ResourceCache] Cache cleared ({} entries)", count);
    }

private:
    void EvictOne()
    {
        if (m_LruList.empty())
            return;

        const CacheEntry& entry = m_LruList.back();
        m_LruMap.erase(entry.id);
        m_LruList.pop_back();
    }

private:
    size_t                                                                 m_MaxSize = 1000;
    std::list<CacheEntry>                                                  m_LruList;
    std::unordered_map<uint64_t, typename std::list<CacheEntry>::iterator> m_LruMap;
};

} // namespace LXEngine
