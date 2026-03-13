#include "Resource/RuntimeMesh.h"

#include "Core/Logging/LogMacros.h"

namespace LongXi
{

namespace
{

struct RuntimeVertex
{
    Vector3  position{0.0f, 0.0f, 0.0f};
    Vector3  normal{0.0f, 0.0f, 1.0f};
    Vector2  uv{0.0f, 0.0f};
    uint32_t boneIndices[4] = {0, 0, 0, 0};
    float    boneWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
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
            out.normal = vertex.normal;
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

    m_SubMeshes.clear();
    m_SubMeshes.push_back({0, m_IndexCount});

    return true;
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
}

} // namespace LongXi
