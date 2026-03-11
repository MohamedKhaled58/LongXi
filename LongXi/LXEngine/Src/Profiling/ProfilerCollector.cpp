#include "Profiling/ProfilerCollector.h"

#include "Core/Logging/LogMacros.h"

#include <algorithm>

namespace LongXi
{

std::atomic<ProfilerCollector*> ProfilerCollector::s_ActiveCollector = nullptr;

void ProfilerCollector::Initialize()
{
    std::lock_guard<std::mutex> lock(m_EntriesMutex);
    m_Initialized = true;
#if defined(LX_DEBUG) || defined(LX_DEV)
    m_Enabled = true;
#else
    m_Enabled = false;
#endif
    m_FrameActive = false;
    m_ActiveFrameIndex = 0;
    m_ActiveEntries.clear();
    m_LastFrameSnapshot = {};
}

void ProfilerCollector::Shutdown()
{
    if (s_ActiveCollector.load(std::memory_order_relaxed) == this)
    {
        s_ActiveCollector.store(nullptr, std::memory_order_relaxed);
    }

    std::lock_guard<std::mutex> lock(m_EntriesMutex);
    m_Initialized = false;
    m_Enabled = false;
    m_FrameActive = false;
    m_ActiveFrameIndex = 0;
    m_ActiveEntries.clear();
    m_LastFrameSnapshot = {};
}

bool ProfilerCollector::IsInitialized() const
{
    return m_Initialized;
}

bool ProfilerCollector::IsFrameActive() const
{
    return m_FrameActive;
}

void ProfilerCollector::BeginFrame(uint64_t frameIndex)
{
    std::lock_guard<std::mutex> lock(m_EntriesMutex);
    if (!m_Initialized)
    {
        LX_ENGINE_WARN("[Profiler] BeginFrame rejected: collector not initialized");
        return;
    }

    if (!m_Enabled)
    {
        return;
    }

    if (m_FrameActive)
    {
        LX_ENGINE_WARN("[Profiler] BeginFrame rejected: frame {} already active", m_ActiveFrameIndex);
        return;
    }

    m_ActiveFrameIndex = frameIndex;
    m_ActiveEntries.clear();
    m_FrameActive = true;
}

void ProfilerCollector::EndFrame(uint64_t frameIndex, double frameTimeSeconds)
{
    std::lock_guard<std::mutex> lock(m_EntriesMutex);
    if (!m_Initialized)
    {
        LX_ENGINE_WARN("[Profiler] EndFrame rejected: collector not initialized");
        return;
    }

    if (!m_Enabled)
    {
        return;
    }

    if (!m_FrameActive)
    {
        LX_ENGINE_WARN("[Profiler] EndFrame rejected: no active frame");
        return;
    }

    if (frameIndex != m_ActiveFrameIndex)
    {
        LX_ENGINE_WARN("[Profiler] EndFrame rejected: frame index mismatch (expected={}, actual={})", m_ActiveFrameIndex, frameIndex);
        m_ActiveEntries.clear();
        m_FrameActive = false;
        m_ActiveFrameIndex = 0;
        return;
    }

    m_LastFrameSnapshot.FrameIndex = frameIndex;
    m_LastFrameSnapshot.FrameTimeMicroseconds = static_cast<uint64_t>(std::max(0.0, frameTimeSeconds) * 1000000.0);
    m_LastFrameSnapshot.Entries.clear();
    m_LastFrameSnapshot.Entries.reserve(m_ActiveEntries.size());

    for (const auto& pair : m_ActiveEntries)
    {
        m_LastFrameSnapshot.Entries.push_back(pair.second);
    }

    std::ranges::sort(m_LastFrameSnapshot.Entries,
                      [](const FrameProfileEntry& lhs, const FrameProfileEntry& rhs)
                      {
                          if (lhs.TotalDurationMicroseconds == rhs.TotalDurationMicroseconds)
                          {
                              return lhs.ScopeName < rhs.ScopeName;
                          }
                          return lhs.TotalDurationMicroseconds > rhs.TotalDurationMicroseconds;
                      });

    m_ActiveEntries.clear();
    m_FrameActive = false;
    m_ActiveFrameIndex = 0;
}

void ProfilerCollector::RecordScope(const char* scopeName, uint64_t durationMicroseconds)
{
    std::lock_guard<std::mutex> lock(m_EntriesMutex);
    if (!m_Initialized)
    {
        LX_ENGINE_WARN("[Profiler] ScopeRecord rejected: collector not initialized");
        return;
    }

    if (!m_Enabled)
    {
        return;
    }

    if (!m_FrameActive || !scopeName || scopeName[0] == '\0')
    {
        return;
    }

    FrameProfileEntry& entry = m_ActiveEntries[scopeName];
    if (entry.ScopeName.empty())
    {
        entry.ScopeName = scopeName;
    }

    entry.TotalDurationMicroseconds += durationMicroseconds;
    ++entry.CallCount;
}

const FrameProfileSnapshot& ProfilerCollector::GetLastFrameSnapshot() const
{
    return m_LastFrameSnapshot;
}

void ProfilerCollector::SetActiveCollector(ProfilerCollector* collector)
{
    s_ActiveCollector.store(collector, std::memory_order_relaxed);
}

ProfilerCollector* ProfilerCollector::GetActiveCollector()
{
    return s_ActiveCollector.load(std::memory_order_relaxed);
}

} // namespace LongXi
