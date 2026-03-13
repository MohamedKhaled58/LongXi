#pragma once

#include <cstdint>
#include <functional>

#include "Core/Graphics/TextureFormat.h"

namespace LXEngine
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

enum class RendererResourceUsage : uint8_t
{
    Static = 0,
    Dynamic,
    Staging,
};

enum class RendererCpuAccessFlags : uint8_t
{
    None      = 0,
    Read      = 1 << 0,
    Write     = 1 << 1,
    ReadWrite = 3,
};

enum class RendererBindFlags : uint32_t
{
    None           = 0,
    VertexBuffer   = 1 << 0,
    IndexBuffer    = 1 << 1,
    ConstantBuffer = 1 << 2,
    ShaderResource = 1 << 3,
    RenderTarget   = 1 << 4,
    DepthStencil   = 1 << 5,
};

enum class RendererShaderStage : uint8_t
{
    Vertex = 0,
    Pixel,
};

enum class RendererBufferType : uint8_t
{
    Vertex = 0,
    Index,
    Constant,
};

enum class RendererIndexFormat : uint8_t
{
    UInt16 = 0,
    UInt32,
};

enum class RendererMapMode : uint8_t
{
    WriteDiscard = 0,
    WriteNoOverwrite,
    Read,
};

enum class RendererResultCode : uint8_t
{
    Success = 0,
    NotInitialized,
    InvalidArgument,
    InvalidDescriptor,
    InvalidHandle,
    InvalidOperation,
    Unsupported,
    BackendFailure,
};

inline RendererBindFlags operator|(RendererBindFlags lhs, RendererBindFlags rhs)
{
    return static_cast<RendererBindFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RendererBindFlags operator&(RendererBindFlags lhs, RendererBindFlags rhs)
{
    return static_cast<RendererBindFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline RendererCpuAccessFlags operator|(RendererCpuAccessFlags lhs, RendererCpuAccessFlags rhs)
{
    return static_cast<RendererCpuAccessFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline bool HasAnyBindFlag(RendererBindFlags value, RendererBindFlags flag)
{
    return (value & flag) != RendererBindFlags::None;
}

inline bool HasAnyCpuAccess(RendererCpuAccessFlags value, RendererCpuAccessFlags flag)
{
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

struct RendererHandleId
{
    uint32_t Slot       = 0;
    uint32_t Generation = 0;

    bool IsValid() const
    {
        return Slot != 0 && Generation != 0;
    }
};

struct RendererTextureHandle
{
    RendererHandleId Id;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererVertexBufferHandle
{
    RendererHandleId Id;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererIndexBufferHandle
{
    RendererHandleId Id;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererConstantBufferHandle
{
    RendererHandleId Id;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererBufferHandle
{
    RendererHandleId   Id;
    RendererBufferType Type = RendererBufferType::Vertex;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererShaderHandle
{
    RendererHandleId    Id;
    RendererShaderStage Stage = RendererShaderStage::Vertex;

    bool IsValid() const
    {
        return Id.IsValid();
    }
};

struct RendererViewport
{
    float X        = 0.0f;
    float Y        = 0.0f;
    float Width    = 0.0f;
    float Height   = 0.0f;
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

struct RendererResult
{
    RendererResultCode Code = RendererResultCode::Success;

    bool Succeeded() const
    {
        return Code == RendererResultCode::Success;
    }
};

struct RendererTextureDesc
{
    uint32_t               Width           = 0;
    uint32_t               Height          = 0;
    LXCore::TextureFormat  Format          = LXCore::TextureFormat::RGBA8;
    RendererResourceUsage  Usage           = RendererResourceUsage::Static;
    RendererCpuAccessFlags CpuAccess       = RendererCpuAccessFlags::None;
    RendererBindFlags      BindFlags       = RendererBindFlags::ShaderResource;
    const void*            InitialData     = nullptr;
    uint32_t               InitialDataSize = 0;
    uint32_t               InitialRowPitch = 0;
};

struct RendererBufferDesc
{
    RendererBufferType     Type            = RendererBufferType::Vertex;
    uint32_t               ByteSize        = 0;
    uint32_t               Stride          = 0;
    RendererResourceUsage  Usage           = RendererResourceUsage::Static;
    RendererCpuAccessFlags CpuAccess       = RendererCpuAccessFlags::None;
    RendererBindFlags      BindFlags       = RendererBindFlags::None;
    const void*            InitialData     = nullptr;
    uint32_t               InitialDataSize = 0;
};

struct RendererShaderDesc
{
    RendererShaderStage Stage        = RendererShaderStage::Vertex;
    const void*         Bytecode     = nullptr;
    uint32_t            BytecodeSize = 0;
};

struct RendererBufferUpdateRequest
{
    RendererBufferHandle Handle;
    const void*          Data       = nullptr;
    uint32_t             ByteOffset = 0;
    uint32_t             ByteSize   = 0;
};

struct RendererMapRequest
{
    RendererBufferHandle Handle;
    RendererMapMode      Mode = RendererMapMode::WriteDiscard;
};

struct RendererMappedResource
{
    void*    Data       = nullptr;
    uint32_t RowPitch   = 0;
    uint32_t DepthPitch = 0;
};

inline RendererBufferHandle ToBufferHandle(RendererVertexBufferHandle handle)
{
    return {handle.Id, RendererBufferType::Vertex};
}

inline RendererBufferHandle ToBufferHandle(RendererIndexBufferHandle handle)
{
    return {handle.Id, RendererBufferType::Index};
}

inline RendererBufferHandle ToBufferHandle(RendererConstantBufferHandle handle)
{
    return {handle.Id, RendererBufferType::Constant};
}

using ExternalPassCallback = std::function<void()>;

} // namespace LXEngine
