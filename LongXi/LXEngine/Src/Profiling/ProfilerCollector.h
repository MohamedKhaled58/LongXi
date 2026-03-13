#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Profiling/IProfileSink.h"
#include "Core/Profiling/ProfilerTypes.h"

namespace LXEngine
{

using LXCore::FrameProfileEntry;
using LXCore::FrameProfileSnapshot;

class ProfilerCollector : public LXCore::IProfileSink
{
public:
    void Initialize();
    void Shutdown();

    bool IsInitialized() const;
    bool IsFrameActive() const;

    void BeginFrame(uint64_t frameIndex);
    void EndFrame(uint64_t frameIndex, double frameTimeSeconds);

    void RecordScope(const char* scopeName, uint64_t durationMicroseconds) override;

    const FrameProfileSnapshot& GetLastFrameSnapshot() const;

    static void               SetActiveCollector(ProfilerCollector* collector);
    static ProfilerCollector* GetActiveCollector();

private:
    bool     m_Initialized      = false;
    bool     m_Enabled          = false;
    bool     m_FrameActive      = false;
    uint64_t m_ActiveFrameIndex = 0;

    // Scope names are expected to be static literals from LX_PROFILE_SCOPE.
    std::unordered_map<std::string_view, FrameProfileEntry> m_ActiveEntries;
    mutable std::mutex                                      m_EntriesMutex;
    FrameProfileSnapshot                                    m_LastFrameSnapshot = {};

    static std::atomic<ProfilerCollector*> s_ActiveCollector;
};

} // namespace LXEngine
