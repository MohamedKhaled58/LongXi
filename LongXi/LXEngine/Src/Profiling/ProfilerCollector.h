#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace LongXi
{

struct FrameProfileEntry
{
    std::string ScopeName;
    uint64_t    TotalDurationMicroseconds = 0;
    uint32_t    CallCount                 = 0;
};

struct FrameProfileSnapshot
{
    uint64_t                       FrameIndex            = 0;
    uint64_t                       FrameTimeMicroseconds = 0;
    std::vector<FrameProfileEntry> Entries;
};

class ProfilerCollector
{
public:
    void Initialize();
    void Shutdown();

    bool IsInitialized() const;
    bool IsFrameActive() const;

    void BeginFrame(uint64_t frameIndex);
    void EndFrame(uint64_t frameIndex, double frameTimeSeconds);

    void RecordScope(const char* scopeName, uint64_t durationMicroseconds);

    const FrameProfileSnapshot& GetLastFrameSnapshot() const;

    static void               SetActiveCollector(ProfilerCollector* collector);
    static ProfilerCollector* GetActiveCollector();

private:
    bool     m_Initialized      = false;
    bool     m_Enabled          = false;
    bool     m_FrameActive      = false;
    uint64_t m_ActiveFrameIndex = 0;

    // Scope names are expected to be static literals from LX_PROFILE_SCOPE.
    std::unordered_map<const char*, FrameProfileEntry> m_ActiveEntries;
    mutable std::mutex                                 m_EntriesMutex;
    FrameProfileSnapshot                               m_LastFrameSnapshot = {};

    static std::atomic<ProfilerCollector*> s_ActiveCollector;
};

} // namespace LongXi
