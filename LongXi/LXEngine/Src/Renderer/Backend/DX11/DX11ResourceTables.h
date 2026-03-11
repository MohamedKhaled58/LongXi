#pragma once

#include "Renderer/RendererTypes.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

namespace LongXi
{

enum class DX11ResourceRecordState : uint8_t
{
    Allocated = 0,
    Bound,
    Mapped,
    Destroyed,
};

struct DX11TextureRecord
{
    uint32_t Generation = 1;
    DX11ResourceRecordState State = DX11ResourceRecordState::Destroyed;
    RendererResourceUsage Usage = RendererResourceUsage::Static;
    RendererBindFlags BindFlags = RendererBindFlags::None;
    std::string DebugName;
    uint64_t CreatedFrameIndex = 0;
    uint64_t LastBoundFrameIndex = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;

    bool IsLive() const
    {
        return State != DX11ResourceRecordState::Destroyed;
    }
};

struct DX11BufferRecord
{
    uint32_t Generation = 1;
    DX11ResourceRecordState State = DX11ResourceRecordState::Destroyed;
    RendererResourceUsage Usage = RendererResourceUsage::Static;
    RendererCpuAccessFlags CpuAccess = RendererCpuAccessFlags::None;
    RendererBindFlags BindFlags = RendererBindFlags::None;
    RendererBufferType Type = RendererBufferType::Vertex;
    uint32_t ByteSize = 0;
    uint64_t CreatedFrameIndex = 0;
    uint64_t LastBoundFrameIndex = 0;
    bool IsMapped = false;
    std::string DebugName;
    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;

    bool IsLive() const
    {
        return State != DX11ResourceRecordState::Destroyed;
    }
};

struct DX11ShaderRecord
{
    uint32_t Generation = 1;
    DX11ResourceRecordState State = DX11ResourceRecordState::Destroyed;
    RendererShaderStage Stage = RendererShaderStage::Vertex;
    std::string DebugName;
    uint64_t CreatedFrameIndex = 0;
    uint64_t LastBoundFrameIndex = 0;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;

    bool IsLive() const
    {
        return State != DX11ResourceRecordState::Destroyed;
    }
};

struct DX11ResourcePoolStats
{
    uint32_t Created = 0;
    uint32_t Destroyed = 0;
    uint32_t ForceReleased = 0;
    uint32_t Live = 0;
};

class DX11ResourceTables
{
public:
    RendererTextureHandle RegisterTexture(const DX11TextureRecord& recordTemplate);
    RendererVertexBufferHandle RegisterVertexBuffer(const DX11BufferRecord& recordTemplate);
    RendererIndexBufferHandle RegisterIndexBuffer(const DX11BufferRecord& recordTemplate);
    RendererConstantBufferHandle RegisterConstantBuffer(const DX11BufferRecord& recordTemplate);
    RendererShaderHandle RegisterShader(const DX11ShaderRecord& recordTemplate);

    bool ResolveTexture(RendererTextureHandle handle, DX11TextureRecord*& outRecord, RendererResultCode& outError);
    bool ResolveTexture(RendererTextureHandle handle, const DX11TextureRecord*& outRecord, RendererResultCode& outError) const;

    bool ResolveBuffer(RendererBufferHandle handle, DX11BufferRecord*& outRecord, RendererResultCode& outError);
    bool ResolveBuffer(RendererBufferHandle handle, const DX11BufferRecord*& outRecord, RendererResultCode& outError) const;

    bool ResolveShader(RendererShaderHandle handle, DX11ShaderRecord*& outRecord, RendererResultCode& outError);
    bool ResolveShader(RendererShaderHandle handle, const DX11ShaderRecord*& outRecord, RendererResultCode& outError) const;

    bool DestroyTexture(RendererTextureHandle handle, RendererResultCode& outError);
    bool DestroyBuffer(RendererBufferHandle handle, RendererResultCode& outError);
    bool DestroyShader(RendererShaderHandle handle, RendererResultCode& outError);

    void MarkTextureBound(RendererTextureHandle handle, uint64_t frameIndex);
    void MarkBufferBound(RendererBufferHandle handle, uint64_t frameIndex);
    void MarkShaderBound(RendererShaderHandle handle, uint64_t frameIndex);
    bool MarkBufferMapped(RendererBufferHandle handle, RendererResultCode& outError);
    bool MarkBufferUnmapped(RendererBufferHandle handle, RendererResultCode& outError);

    void ReleaseAll();

    const DX11ResourcePoolStats& GetTexturePoolStats() const
    {
        return m_TextureStats;
    }

    const DX11ResourcePoolStats& GetBufferPoolStats() const
    {
        return m_BufferStats;
    }

    const DX11ResourcePoolStats& GetShaderPoolStats() const
    {
        return m_ShaderStats;
    }

private:
    RendererHandleId AllocateSlot(std::vector<uint32_t>& freeList, uint32_t& liveCount, uint32_t& createdCount, std::vector<uint32_t>& generations);
    void DestroySlot(RendererHandleId id, std::vector<uint32_t>& freeList, uint32_t& liveCount, uint32_t& destroyedCount);

private:
    std::vector<DX11TextureRecord> m_Textures;
    std::vector<DX11BufferRecord> m_Buffers;
    std::vector<DX11ShaderRecord> m_Shaders;

    std::vector<uint32_t> m_TextureGenerations;
    std::vector<uint32_t> m_BufferGenerations;
    std::vector<uint32_t> m_ShaderGenerations;

    std::vector<uint32_t> m_TextureFreeList;
    std::vector<uint32_t> m_BufferFreeList;
    std::vector<uint32_t> m_ShaderFreeList;

    DX11ResourcePoolStats m_TextureStats;
    DX11ResourcePoolStats m_BufferStats;
    DX11ResourcePoolStats m_ShaderStats;
};

} // namespace LongXi
