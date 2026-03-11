#pragma once

#include <chrono>
#include <cstdint>

namespace LongXi
{

struct TimingSnapshot
{
    uint64_t FrameIndex = 0;
    double DeltaTimeSeconds = 0.0;
    double FrameTimeSeconds = 0.0;
    double TotalTimeSeconds = 0.0;
};

class TimingService
{
public:
    using Clock = std::chrono::steady_clock;

    void Initialize();
    void Shutdown();

    bool IsInitialized() const;
    bool IsFrameActive() const;

    void BeginFrame();
    void EndFrame();

    const TimingSnapshot& GetSnapshot() const;

private:
    bool m_Initialized = false;
    bool m_FrameActive = false;
    bool m_HasPreviousFrameTimestamp = false;

    Clock::time_point m_PreviousFrameTimestamp = {};
    Clock::time_point m_CurrentFrameStartTimestamp = {};

    TimingSnapshot m_Snapshot = {};
};

} // namespace LongXi
