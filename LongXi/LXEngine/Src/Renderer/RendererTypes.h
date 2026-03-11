#pragma once

#include <cstdint>
#include <functional>
#include <memory>

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

struct RendererTextureHandle
{
    std::shared_ptr<void> NativeResource;

    bool IsValid() const
    {
        return static_cast<bool>(NativeResource);
    }
};

struct RendererBufferHandle
{
    std::shared_ptr<void> NativeResource;
};

struct RendererShaderHandle
{
    std::shared_ptr<void> NativeResource;
};

struct RendererViewport
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    float MinDepth = 0.0f;
    float MaxDepth = 1.0f;
};

struct RendererColor
{
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float A = 1.0f;
};

using ExternalPassCallback = std::function<void()>;

} // namespace LongXi
