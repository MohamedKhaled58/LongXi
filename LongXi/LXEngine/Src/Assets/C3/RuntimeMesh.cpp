#include "Assets/C3/RuntimeMesh.h"

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

namespace
{

struct RuntimeVertex
{
    LXCore::Vector3 position{0.0f, 0.0f, 0.0f};
    LXCore::Vector3 normal{0.0f, 0.0f, 1.0f};
    LXCore::Vector2 uv{0.0f, 0.0f};
    uint32_t        boneIndices[4] = {0, 0, 0, 0};
    float           boneWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t        hasNormal      = 0;
};

} // namespace

bool RuntimeMesh::Initialize(Renderer& renderer, const MeshResource& resource)
{
    Release(renderer);

    if (resource.vertices.empty() || resource.indices.empty())
    {
        LX_ENGINE_WARN("[RuntimeMesh] Missing vertex or index data");
        return false;
    }

    std::vector<RuntimeVertex> vertices;
    vertices.reserve(resource.vertices.size());
    for (const auto& vertex : resource.vertices)
    {
        RuntimeVertex out;
        out.position = vertex.position;
        if (vertex.hasNormal)
        {
            out.normal    = vertex.normal;
            out.hasNormal = 1;
        }
        out.uv = vertex.uv;
        for (size_t i = 0; i < vertex.skinning.indices.size(); ++i)
        {
            out.boneIndices[i] = vertex.skinning.indices[i];
            out.boneWeights[i] = vertex.skinning.weights[i];
        }
        vertices.push_back(out);
    }

    RendererBufferDesc vertexDesc;
    vertexDesc.Type            = RendererBufferType::Vertex;
    vertexDesc.ByteSize        = static_cast<uint32_t>(vertices.size() * sizeof(RuntimeVertex));
    vertexDesc.Stride          = sizeof(RuntimeVertex);
    vertexDesc.Usage           = RendererResourceUsage::Static;
    vertexDesc.CpuAccess       = RendererCpuAccessFlags::None;
    vertexDesc.BindFlags       = RendererBindFlags::VertexBuffer;
    vertexDesc.InitialData     = vertices.data();
    vertexDesc.InitialDataSize = vertexDesc.ByteSize;

    m_VertexBuffer = renderer.CreateVertexBuffer(vertexDesc);
    if (!m_VertexBuffer.IsValid())
    {
        LX_ENGINE_WARN("[RuntimeMesh] Failed to create vertex buffer");
        return false;
    }

    RendererBufferDesc indexDesc;
    indexDesc.Type            = RendererBufferType::Index;
    indexDesc.ByteSize        = static_cast<uint32_t>(resource.indices.size() * sizeof(uint32_t));
    indexDesc.Stride          = sizeof(uint32_t);
    indexDesc.Usage           = RendererResourceUsage::Static;
    indexDesc.CpuAccess       = RendererCpuAccessFlags::None;
    indexDesc.BindFlags       = RendererBindFlags::IndexBuffer;
    indexDesc.InitialData     = resource.indices.data();
    indexDesc.InitialDataSize = indexDesc.ByteSize;

    m_IndexBuffer = renderer.CreateIndexBuffer(indexDesc);
    if (!m_IndexBuffer.IsValid())
    {
        LX_ENGINE_WARN("[RuntimeMesh] Failed to create index buffer");
        renderer.DestroyBuffer(ToBufferHandle(m_VertexBuffer));
        m_VertexBuffer = {};
        return false;
    }

    m_IndexCount  = static_cast<uint32_t>(resource.indices.size());
    m_IndexFormat = RendererIndexFormat::UInt32;

    m_SlotName = resource.name;
    m_InitMatrix = resource.initMatrix;

    m_SubMeshes.clear();
    if (!resource.subsets.empty())
    {
        m_SubMeshes.reserve(resource.subsets.size());
        for (const auto& subset : resource.subsets)
        {
            m_SubMeshes.push_back({subset.startIndex, subset.indexCount, subset.isTransparent});
        }
    }
    else
    {
        m_SubMeshes.push_back({0, m_IndexCount, false});
    }

    return true;
}

const std::string& RuntimeMesh::GetSlotName() const
{
    return m_SlotName;
}

void RuntimeMesh::SetSlotName(const std::string& name)
{
    m_SlotName = name;
}

void RuntimeMesh::SetDefaultTexture(RendererTextureHandle handle)
{
    m_DefaultTexture = handle;
}

void RuntimeMesh::OverrideTexture(RendererTextureHandle handle)
{
    m_OverrideTexture = handle;
}

void RuntimeMesh::ClearTextureOverride()
{
    m_OverrideTexture = {};
}

RendererTextureHandle RuntimeMesh::GetActiveTexture() const
{
    return m_OverrideTexture.IsValid() ? m_OverrideTexture : m_DefaultTexture;
}

void RuntimeMesh::Release(Renderer& renderer)
{
    if (m_VertexBuffer.IsValid())
    {
        renderer.DestroyBuffer(ToBufferHandle(m_VertexBuffer));
        m_VertexBuffer = {};
    }

    if (m_IndexBuffer.IsValid())
    {
        renderer.DestroyBuffer(ToBufferHandle(m_IndexBuffer));
        m_IndexBuffer = {};
    }

    m_SubMeshes.clear();
    m_IndexCount = 0;
    m_SlotName.clear();
    m_DefaultTexture  = {};
    m_OverrideTexture = {};
}

} // namespace LXEngine
