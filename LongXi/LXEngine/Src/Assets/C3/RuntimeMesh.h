#pragma once

#include <cstdint>
#include <vector>

#include "Assets/C3/Resources.h"
#include "Renderer/Renderer.h"
#include "Renderer/RendererTypes.h"

namespace LXEngine
{

struct RuntimeSubMesh
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
};

class RuntimeMesh
{
public:
    bool Initialize(Renderer& renderer, const MeshResource& resource);
    void Release(Renderer& renderer);

    bool IsValid() const
    {
        return m_VertexBuffer.IsValid() && m_IndexBuffer.IsValid();
    }

    RendererVertexBufferHandle GetVertexBuffer() const
    {
        return m_VertexBuffer;
    }

    RendererIndexBufferHandle GetIndexBuffer() const
    {
        return m_IndexBuffer;
    }

    RendererIndexFormat GetIndexFormat() const
    {
        return m_IndexFormat;
    }

    uint32_t GetIndexCount() const
    {
        return m_IndexCount;
    }

    const std::vector<RuntimeSubMesh>& GetSubMeshes() const
    {
        return m_SubMeshes;
    }

private:
    RendererVertexBufferHandle  m_VertexBuffer{};
    RendererIndexBufferHandle   m_IndexBuffer{};
    RendererIndexFormat         m_IndexFormat = RendererIndexFormat::UInt32;
    uint32_t                    m_IndexCount  = 0;
    std::vector<RuntimeSubMesh> m_SubMeshes;
};

} // namespace LXEngine
