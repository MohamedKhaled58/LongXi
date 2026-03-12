#pragma once

#include <chrono>
#include <cstdint>

namespace LongXi
{

struct TimingSnapshot
{
    uint64_t FrameIndex               = 0;
    double   UnscaledDeltaTimeSeconds = 0.0;
    double   DeltaTimeSeconds         = 0.0;
    double   FrameTimeSeconds         = 0.0;
    double   TotalTimeSeconds         = 0.0;
    bool     bDeltaTimeClamped        = false;
};

class TimingService
{
public:
    using Clock = std::chrono::steady_clock;

    void Initialize();
    void Shutdown();

    bool IsInitialized() const;
    bool IsFrameActive() const;

    void SetFrameLimiterEnabled(bool enabled);
    bool IsFrameLimiterEnabled() const;

    void   SetFrameRateLimit(double targetFramesPerSecond);
    double GetFrameRateLimit() const;

    void   SetMaxDeltaTime(double maxDeltaSeconds);
    double GetMaxDeltaTime() const;

    void BeginFrame();
    void EndFrame();

    const TimingSnapshot& GetSnapshot() const;

private:
    bool m_Initialized               = false;
    bool m_FrameActive               = false;
    bool m_HasPreviousFrameTimestamp = false;
    bool m_FrameLimiterEnabled       = true;

    Clock::time_point m_PreviousFrameTimestamp     = {};
    Clock::time_point m_CurrentFrameStartTimestamp = {};

    double m_TargetFramesPerSecond           = 60.0;
    double m_TargetFrameDurationSeconds      = 1.0 / 60.0;
    double m_MaxDeltaSeconds                 = 0.100;
    double m_LargeGapWarningThresholdSeconds = 0.250;

    void EnforceFrameRateLimit();

    TimingSnapshot m_Snapshot = {};
};

} // namespace LongXi
