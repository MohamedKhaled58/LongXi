#pragma once

#include <chrono>
#include <cstdint>

#include "Core/Profiling/IProfileSink.h"

namespace LXCore
{

#if defined(LX_DEBUG) || defined(LX_DEV)
static constexpr bool LX_PROFILING_ENABLED = true;
#else
static constexpr bool LX_PROFILING_ENABLED = false;
#endif

class ProfileScopeGuard
{
public:
    explicit ProfileScopeGuard(const char* scopeName)
        : m_ScopeName(scopeName)
        , m_Sink(LX_PROFILING_ENABLED ? IProfileSink::GetActive() : nullptr)
    {
        if (m_Sink && m_ScopeName && m_ScopeName[0] != '\0')
        {
            m_StartTimestamp = Clock::now();
            m_Active         = true;
        }
    }

    ~ProfileScopeGuard()
    {
        if (!m_Active || !m_Sink)
        {
            return;
        }

        const Clock::time_point endTimestamp = Clock::now();
        const uint64_t          elapsedMicros =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(endTimestamp - m_StartTimestamp).count());
        m_Sink->RecordScope(m_ScopeName, elapsedMicros);
    }

    ProfileScopeGuard(const ProfileScopeGuard&)            = delete;
    ProfileScopeGuard& operator=(const ProfileScopeGuard&) = delete;

private:
    using Clock = std::chrono::steady_clock;

    const char*       m_ScopeName      = nullptr;
    IProfileSink*     m_Sink           = nullptr;
    Clock::time_point m_StartTimestamp = {};
    bool              m_Active         = false;
};

} // namespace LXCore

#define LX_CONCAT_IMPL(left, right) left##right
#define LX_CONCAT(left, right) LX_CONCAT_IMPL(left, right)

#if defined(LX_DEBUG) || defined(LX_DEV)
#define LX_PROFILE_SCOPE(scopeName) ::LXCore::ProfileScopeGuard LX_CONCAT(LX_ProfileScopeGuard_, __LINE__)(scopeName)
#else
#define LX_PROFILE_SCOPE(scopeName) \
    do                              \
    {                               \
    } while (0)
#endif
