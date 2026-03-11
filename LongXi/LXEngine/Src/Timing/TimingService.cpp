#include "Timing/TimingService.h"

#include "Core/Logging/LogMacros.h"
#include "Profiling/ProfileScope.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace LongXi
{

void TimingService::Initialize()
{
    m_Initialized = true;
    m_FrameActive = false;
    m_HasPreviousFrameTimestamp = false;
    m_PreviousFrameTimestamp = {};
    m_CurrentFrameStartTimestamp = {};
    m_Snapshot = {};
}

void TimingService::Shutdown()
{
    m_Initialized = false;
    m_FrameActive = false;
    m_HasPreviousFrameTimestamp = false;
    m_PreviousFrameTimestamp = {};
    m_CurrentFrameStartTimestamp = {};
    m_Snapshot = {};
}

bool TimingService::IsInitialized() const
{
    return m_Initialized;
}

bool TimingService::IsFrameActive() const
{
    return m_FrameActive;
}

void TimingService::SetFrameLimiterEnabled(bool enabled)
{
    m_FrameLimiterEnabled = enabled;
}

bool TimingService::IsFrameLimiterEnabled() const
{
    return m_FrameLimiterEnabled;
}

void TimingService::SetFrameRateLimit(double targetFramesPerSecond)
{
    if (!std::isfinite(targetFramesPerSecond) || targetFramesPerSecond <= 0.0)
    {
        LX_ENGINE_WARN("[Timing] Invalid frame rate limit requested ({:.3f}). Keeping {:.3f} FPS", targetFramesPerSecond, m_TargetFramesPerSecond);
        return;
    }

    m_TargetFramesPerSecond = targetFramesPerSecond;
    m_TargetFrameDurationSeconds = 1.0 / m_TargetFramesPerSecond;
}

double TimingService::GetFrameRateLimit() const
{
    return m_TargetFramesPerSecond;
}

void TimingService::SetMaxDeltaTime(double maxDeltaSeconds)
{
    if (!std::isfinite(maxDeltaSeconds) || maxDeltaSeconds <= 0.0)
    {
        LX_ENGINE_WARN("[Timing] Invalid max delta requested ({:.6f}). Keeping {:.6f} seconds", maxDeltaSeconds, m_MaxDeltaSeconds);
        return;
    }

    m_MaxDeltaSeconds = maxDeltaSeconds;
}

double TimingService::GetMaxDeltaTime() const
{
    return m_MaxDeltaSeconds;
}

void TimingService::BeginFrame()
{
    if (!m_Initialized || m_FrameActive)
    {
        return;
    }

    EnforceFrameRateLimit();

    const Clock::time_point now = Clock::now();

    double unscaledDeltaSeconds = 0.0;
    if (m_HasPreviousFrameTimestamp)
    {
        unscaledDeltaSeconds = std::max(0.0, std::chrono::duration<double>(now - m_PreviousFrameTimestamp).count());
    }

    const double clampedDeltaSeconds = std::min(unscaledDeltaSeconds, m_MaxDeltaSeconds);
    const bool bDeltaTimeClamped = clampedDeltaSeconds < unscaledDeltaSeconds;
    if (bDeltaTimeClamped && unscaledDeltaSeconds >= m_LargeGapWarningThresholdSeconds)
    {
        LX_ENGINE_WARN("[Timing] Large frame gap detected. Unscaled {:.3f} ms clamped to {:.3f} ms",
                       unscaledDeltaSeconds * 1000.0,
                       clampedDeltaSeconds * 1000.0);
    }

    m_PreviousFrameTimestamp = now;
    m_HasPreviousFrameTimestamp = true;
    m_CurrentFrameStartTimestamp = now;
    m_FrameActive = true;

    ++m_Snapshot.FrameIndex;
    m_Snapshot.UnscaledDeltaTimeSeconds = unscaledDeltaSeconds;
    m_Snapshot.DeltaTimeSeconds = clampedDeltaSeconds;
    m_Snapshot.TotalTimeSeconds += unscaledDeltaSeconds;
    m_Snapshot.FrameTimeSeconds = 0.0;
    m_Snapshot.bDeltaTimeClamped = bDeltaTimeClamped;
}

void TimingService::EndFrame()
{
    if (!m_Initialized || !m_FrameActive)
    {
        return;
    }

    const Clock::time_point now = Clock::now();
    const double frameSeconds = std::max(0.0, std::chrono::duration<double>(now - m_CurrentFrameStartTimestamp).count());
    m_Snapshot.FrameTimeSeconds = frameSeconds;
    m_FrameActive = false;
}

const TimingSnapshot& TimingService::GetSnapshot() const
{
    return m_Snapshot;
}

void TimingService::EnforceFrameRateLimit()
{
    if (!m_FrameLimiterEnabled || !m_HasPreviousFrameTimestamp)
    {
        return;
    }

    LX_PROFILE_SCOPE("Timing.FrameLimiterWait");

    const std::chrono::duration<double> targetDurationSeconds(m_TargetFrameDurationSeconds);
    const Clock::duration targetFrameDuration = std::chrono::duration_cast<Clock::duration>(targetDurationSeconds);
    const Clock::time_point targetFrameStart = m_PreviousFrameTimestamp + targetFrameDuration;

    while (true)
    {
        const Clock::time_point now = Clock::now();
        if (now >= targetFrameStart)
        {
            return;
        }

        const Clock::duration remaining = targetFrameStart - now;
        if (remaining > std::chrono::milliseconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::this_thread::yield();
    }
}

} // namespace LongXi
