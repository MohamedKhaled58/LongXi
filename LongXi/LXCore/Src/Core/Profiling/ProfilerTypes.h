#pragma once

#include <cstdint>
#include <string>
#include <vector>

// =============================================================================
// ProfilerTypes — Shared profiling data types (moved from LXEngine)
// FrameProfileEntry and FrameProfileSnapshot are used for profiling data.
// =============================================================================

namespace LXCore
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

} // namespace LXCore
