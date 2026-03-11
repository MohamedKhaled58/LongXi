#pragma once

#include <functional>

namespace LongXi
{

enum class RenderPassType
{
    None = 0,
    Scene,
    Sprite,
    DebugUI,
    External,
};

enum class FrameLifecyclePhase
{
    NotStarted = 0,
    InFrame,
    InPass,
    FrameEnded,
};

enum class RendererRecoveryMode
{
    Normal = 0,
    RecoveryPending,
    SafeNoRender,
    Reinitializing,
};

using ExternalPassCallback = std::function<void()>;

} // namespace LongXi
