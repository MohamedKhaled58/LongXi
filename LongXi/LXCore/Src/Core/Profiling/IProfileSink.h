#pragma once

#include <atomic>

// =============================================================================
// IProfileSink — Abstract profiling sink interface
// Allows LXCore code to emit profiling events without depending on LXEngine.
// Implemented by ProfilerCollector in LXEngine.
// =============================================================================

namespace LXCore
{

class IProfileSink
{
public:
    virtual ~IProfileSink() = default;

    virtual void RecordScope(const char* scopeName, uint64_t durationMicroseconds) = 0;

    static void          SetActive(IProfileSink* sink);
    static IProfileSink* GetActive();

private:
    static std::atomic<IProfileSink*> s_Active;
};

} // namespace LXCore
