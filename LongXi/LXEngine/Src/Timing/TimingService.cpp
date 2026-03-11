#include "Timing/TimingService.h"

#include <algorithm>

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

void TimingService::BeginFrame()
{
    if (!m_Initialized || m_FrameActive)
    {
        return;
    }

    const Clock::time_point now = Clock::now();

    double deltaSeconds = 0.0;
    if (m_HasPreviousFrameTimestamp)
    {
        deltaSeconds = std::max(0.0, std::chrono::duration<double>(now - m_PreviousFrameTimestamp).count());
    }

    m_PreviousFrameTimestamp = now;
    m_HasPreviousFrameTimestamp = true;
    m_CurrentFrameStartTimestamp = now;
    m_FrameActive = true;

    ++m_Snapshot.FrameIndex;
    m_Snapshot.DeltaTimeSeconds = deltaSeconds;
    m_Snapshot.TotalTimeSeconds += deltaSeconds;
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

} // namespace LongXi
