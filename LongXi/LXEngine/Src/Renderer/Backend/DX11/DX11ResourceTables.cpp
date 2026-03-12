#include "Renderer/Backend/DX11/DX11ResourceTables.h"

namespace LongXi
{

RendererHandleId DX11ResourceTables::AllocateSlot(std::vector<uint32_t>& freeList,
                                                  uint32_t&              liveCount,
                                                  uint32_t&              createdCount,
                                                  std::vector<uint32_t>& generations)
{
    uint32_t slot = 0;

    if (!freeList.empty())
    {
        slot = freeList.back();
        freeList.pop_back();
    }
    else
    {
        slot = static_cast<uint32_t>(generations.size() + 1);
        generations.push_back(1);
    }

    ++liveCount;
    ++createdCount;

    return {slot, generations[slot - 1]};
}

void DX11ResourceTables::DestroySlot(RendererHandleId id, std::vector<uint32_t>& freeList, uint32_t& liveCount, uint32_t& destroyedCount)
{
    if (id.Slot == 0)
    {
        return;
    }

    if (liveCount > 0)
    {
        --liveCount;
    }

    ++destroyedCount;
    freeList.push_back(id.Slot);
}

RendererTextureHandle DX11ResourceTables::RegisterTexture(const DX11TextureRecord& recordTemplate)
{
    const RendererHandleId id = AllocateSlot(m_TextureFreeList, m_TextureStats.Live, m_TextureStats.Created, m_TextureGenerations);

    if (m_Textures.size() < id.Slot)
    {
        m_Textures.resize(id.Slot);
    }

    DX11TextureRecord& record = m_Textures[id.Slot - 1];
    record                    = recordTemplate;
    record.Generation         = id.Generation;
    record.State              = DX11ResourceRecordState::Allocated;

    return {id};
}

RendererVertexBufferHandle DX11ResourceTables::RegisterVertexBuffer(const DX11BufferRecord& recordTemplate)
{
    const RendererHandleId id = AllocateSlot(m_BufferFreeList, m_BufferStats.Live, m_BufferStats.Created, m_BufferGenerations);

    if (m_Buffers.size() < id.Slot)
    {
        m_Buffers.resize(id.Slot);
    }

    DX11BufferRecord& record = m_Buffers[id.Slot - 1];
    record                   = recordTemplate;
    record.Generation        = id.Generation;
    record.Type              = RendererBufferType::Vertex;
    record.State             = DX11ResourceRecordState::Allocated;

    return {id};
}

RendererIndexBufferHandle DX11ResourceTables::RegisterIndexBuffer(const DX11BufferRecord& recordTemplate)
{
    const RendererHandleId id = AllocateSlot(m_BufferFreeList, m_BufferStats.Live, m_BufferStats.Created, m_BufferGenerations);

    if (m_Buffers.size() < id.Slot)
    {
        m_Buffers.resize(id.Slot);
    }

    DX11BufferRecord& record = m_Buffers[id.Slot - 1];
    record                   = recordTemplate;
    record.Generation        = id.Generation;
    record.Type              = RendererBufferType::Index;
    record.State             = DX11ResourceRecordState::Allocated;

    return {id};
}

RendererConstantBufferHandle DX11ResourceTables::RegisterConstantBuffer(const DX11BufferRecord& recordTemplate)
{
    const RendererHandleId id = AllocateSlot(m_BufferFreeList, m_BufferStats.Live, m_BufferStats.Created, m_BufferGenerations);

    if (m_Buffers.size() < id.Slot)
    {
        m_Buffers.resize(id.Slot);
    }

    DX11BufferRecord& record = m_Buffers[id.Slot - 1];
    record                   = recordTemplate;
    record.Generation        = id.Generation;
    record.Type              = RendererBufferType::Constant;
    record.State             = DX11ResourceRecordState::Allocated;

    return {id};
}

RendererShaderHandle DX11ResourceTables::RegisterShader(const DX11ShaderRecord& recordTemplate)
{
    const RendererHandleId id = AllocateSlot(m_ShaderFreeList, m_ShaderStats.Live, m_ShaderStats.Created, m_ShaderGenerations);

    if (m_Shaders.size() < id.Slot)
    {
        m_Shaders.resize(id.Slot);
    }

    DX11ShaderRecord& record = m_Shaders[id.Slot - 1];
    record                   = recordTemplate;
    record.Generation        = id.Generation;
    record.State             = DX11ResourceRecordState::Allocated;

    return {id, record.Stage};
}

bool DX11ResourceTables::ResolveTexture(RendererTextureHandle handle, DX11TextureRecord*& outRecord, RendererResultCode& outError)
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Textures.size())
    {
        return false;
    }

    DX11TextureRecord& record = m_Textures[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive())
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::ResolveTexture(RendererTextureHandle     handle,
                                        const DX11TextureRecord*& outRecord,
                                        RendererResultCode&       outError) const
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Textures.size())
    {
        return false;
    }

    const DX11TextureRecord& record = m_Textures[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive())
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::ResolveBuffer(RendererBufferHandle handle, DX11BufferRecord*& outRecord, RendererResultCode& outError)
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Buffers.size())
    {
        return false;
    }

    DX11BufferRecord& record = m_Buffers[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive() || record.Type != handle.Type)
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::ResolveBuffer(RendererBufferHandle handle, const DX11BufferRecord*& outRecord, RendererResultCode& outError) const
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Buffers.size())
    {
        return false;
    }

    const DX11BufferRecord& record = m_Buffers[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive() || record.Type != handle.Type)
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::ResolveShader(RendererShaderHandle handle, DX11ShaderRecord*& outRecord, RendererResultCode& outError)
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Shaders.size())
    {
        return false;
    }

    DX11ShaderRecord& record = m_Shaders[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive() || record.Stage != handle.Stage)
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::ResolveShader(RendererShaderHandle handle, const DX11ShaderRecord*& outRecord, RendererResultCode& outError) const
{
    outRecord = nullptr;
    outError  = RendererResultCode::InvalidHandle;

    if (!handle.IsValid())
    {
        return false;
    }

    if (handle.Id.Slot == 0 || handle.Id.Slot > m_Shaders.size())
    {
        return false;
    }

    const DX11ShaderRecord& record = m_Shaders[handle.Id.Slot - 1];
    if (record.Generation != handle.Id.Generation || !record.IsLive() || record.Stage != handle.Stage)
    {
        return false;
    }

    outRecord = &record;
    outError  = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::DestroyTexture(RendererTextureHandle handle, RendererResultCode& outError)
{
    DX11TextureRecord* record = nullptr;
    if (!ResolveTexture(handle, record, outError))
    {
        return false;
    }

    record->ShaderResourceView.Reset();
    record->Texture.Reset();
    record->State = DX11ResourceRecordState::Destroyed;

    if (handle.Id.Slot <= m_TextureGenerations.size())
    {
        uint32_t& generation = m_TextureGenerations[handle.Id.Slot - 1];
        ++generation;
        if (generation == 0)
        {
            generation = 1;
        }
    }

    DestroySlot(handle.Id, m_TextureFreeList, m_TextureStats.Live, m_TextureStats.Destroyed);
    outError = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::DestroyBuffer(RendererBufferHandle handle, RendererResultCode& outError)
{
    DX11BufferRecord* record = nullptr;
    if (!ResolveBuffer(handle, record, outError))
    {
        return false;
    }

    record->Buffer.Reset();
    record->IsMapped = false;
    record->State    = DX11ResourceRecordState::Destroyed;

    if (handle.Id.Slot <= m_BufferGenerations.size())
    {
        uint32_t& generation = m_BufferGenerations[handle.Id.Slot - 1];
        ++generation;
        if (generation == 0)
        {
            generation = 1;
        }
    }

    DestroySlot(handle.Id, m_BufferFreeList, m_BufferStats.Live, m_BufferStats.Destroyed);
    outError = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::DestroyShader(RendererShaderHandle handle, RendererResultCode& outError)
{
    DX11ShaderRecord* record = nullptr;
    if (!ResolveShader(handle, record, outError))
    {
        return false;
    }

    record->VertexShader.Reset();
    record->PixelShader.Reset();
    record->State = DX11ResourceRecordState::Destroyed;

    if (handle.Id.Slot <= m_ShaderGenerations.size())
    {
        uint32_t& generation = m_ShaderGenerations[handle.Id.Slot - 1];
        ++generation;
        if (generation == 0)
        {
            generation = 1;
        }
    }

    DestroySlot(handle.Id, m_ShaderFreeList, m_ShaderStats.Live, m_ShaderStats.Destroyed);
    outError = RendererResultCode::Success;
    return true;
}

void DX11ResourceTables::MarkTextureBound(RendererTextureHandle handle, uint64_t frameIndex)
{
    DX11TextureRecord* record = nullptr;
    RendererResultCode error  = RendererResultCode::Success;
    if (ResolveTexture(handle, record, error))
    {
        record->State               = DX11ResourceRecordState::Bound;
        record->LastBoundFrameIndex = frameIndex;
    }
}

void DX11ResourceTables::MarkBufferBound(RendererBufferHandle handle, uint64_t frameIndex)
{
    DX11BufferRecord*  record = nullptr;
    RendererResultCode error  = RendererResultCode::Success;
    if (ResolveBuffer(handle, record, error))
    {
        record->State               = DX11ResourceRecordState::Bound;
        record->LastBoundFrameIndex = frameIndex;
    }
}

void DX11ResourceTables::MarkShaderBound(RendererShaderHandle handle, uint64_t frameIndex)
{
    DX11ShaderRecord*  record = nullptr;
    RendererResultCode error  = RendererResultCode::Success;
    if (ResolveShader(handle, record, error))
    {
        record->State               = DX11ResourceRecordState::Bound;
        record->LastBoundFrameIndex = frameIndex;
    }
}

bool DX11ResourceTables::MarkBufferMapped(RendererBufferHandle handle, RendererResultCode& outError)
{
    DX11BufferRecord* record = nullptr;
    if (!ResolveBuffer(handle, record, outError))
    {
        return false;
    }

    if (record->IsMapped)
    {
        outError = RendererResultCode::InvalidOperation;
        return false;
    }

    record->IsMapped = true;
    record->State    = DX11ResourceRecordState::Mapped;
    outError         = RendererResultCode::Success;
    return true;
}

bool DX11ResourceTables::MarkBufferUnmapped(RendererBufferHandle handle, RendererResultCode& outError)
{
    DX11BufferRecord* record = nullptr;
    if (!ResolveBuffer(handle, record, outError))
    {
        return false;
    }

    if (!record->IsMapped)
    {
        outError = RendererResultCode::InvalidOperation;
        return false;
    }

    record->IsMapped = false;
    record->State    = DX11ResourceRecordState::Allocated;
    outError         = RendererResultCode::Success;
    return true;
}

void DX11ResourceTables::ReleaseAll()
{
    for (uint32_t i = 0; i < m_Textures.size(); ++i)
    {
        DX11TextureRecord& record = m_Textures[i];
        if (!record.IsLive())
        {
            continue;
        }

        record.ShaderResourceView.Reset();
        record.Texture.Reset();
        record.State = DX11ResourceRecordState::Destroyed;
        ++m_TextureStats.ForceReleased;
        DestroySlot({i + 1, record.Generation}, m_TextureFreeList, m_TextureStats.Live, m_TextureStats.Destroyed);
        if (i < m_TextureGenerations.size())
        {
            ++m_TextureGenerations[i];
            if (m_TextureGenerations[i] == 0)
            {
                m_TextureGenerations[i] = 1;
            }
        }
    }

    for (uint32_t i = 0; i < m_Buffers.size(); ++i)
    {
        DX11BufferRecord& record = m_Buffers[i];
        if (!record.IsLive())
        {
            continue;
        }

        record.Buffer.Reset();
        record.IsMapped = false;
        record.State    = DX11ResourceRecordState::Destroyed;
        ++m_BufferStats.ForceReleased;
        DestroySlot({i + 1, record.Generation}, m_BufferFreeList, m_BufferStats.Live, m_BufferStats.Destroyed);
        if (i < m_BufferGenerations.size())
        {
            ++m_BufferGenerations[i];
            if (m_BufferGenerations[i] == 0)
            {
                m_BufferGenerations[i] = 1;
            }
        }
    }

    for (uint32_t i = 0; i < m_Shaders.size(); ++i)
    {
        DX11ShaderRecord& record = m_Shaders[i];
        if (!record.IsLive())
        {
            continue;
        }

        record.VertexShader.Reset();
        record.PixelShader.Reset();
        record.State = DX11ResourceRecordState::Destroyed;
        ++m_ShaderStats.ForceReleased;
        DestroySlot({i + 1, record.Generation}, m_ShaderFreeList, m_ShaderStats.Live, m_ShaderStats.Destroyed);
        if (i < m_ShaderGenerations.size())
        {
            ++m_ShaderGenerations[i];
            if (m_ShaderGenerations[i] == 0)
            {
                m_ShaderGenerations[i] = 1;
            }
        }
    }
}

} // namespace LongXi
