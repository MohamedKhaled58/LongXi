#include "Resource/C3PhyParser.h"

#include <cstring>
#include <string>

#include "Resource/C3BinaryReader.h"
#include "Resource/C3Log.h"

namespace LongXi
{

namespace
{
#pragma pack(push, 1)

struct PhyVertexFile
{
    Vector3  positions[4];
    float    u;
    float    v;
    uint32_t color;
    uint32_t boneIndices[2];
    float    boneWeights[2];
};

#pragma pack(pop)
static_assert(sizeof(PhyVertexFile) == 76, "PhyVertexFile must be 76 bytes");

#pragma pack(push, 1)

struct Phy4VertexFile
{
    Vector3  position;
    float    u;
    float    v;
    uint32_t color;
    uint32_t boneIndices[2];
    float    boneWeights[2];
};

#pragma pack(pop)
static_assert(sizeof(Phy4VertexFile) == 40, "Phy4VertexFile must be 40 bytes");

bool ReadLengthPrefixedString(C3BinaryReader& reader, std::string& out)
{
    uint32_t length = 0;
    if (!reader.Read(length))
    {
        return false;
    }

    if (length == 0)
    {
        out.clear();
        return true;
    }

    std::vector<uint8_t> bytes;
    if (!reader.ReadBytes(length, bytes))
    {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

} // namespace

bool C3PhyParser::Parse(const std::vector<uint8_t>& data, MeshResource& outMesh, bool isPhy4)
{
    outMesh = MeshResource{};

    C3BinaryReader reader(data);

    std::string name;
    if (!ReadLengthPrefixedString(reader, name))
    {
        LX_C3_WARN("PHY: Failed to read mesh name");
        return false;
    }

    uint32_t blendCount = 0;
    if (!reader.Read(blendCount))
    {
        LX_C3_WARN("PHY: Failed to read blend count (name='{}')", name);
        return false;
    }

    uint32_t normalVertexCount = 0;
    uint32_t alphaVertexCount  = 0;
    if (!reader.Read(normalVertexCount) || !reader.Read(alphaVertexCount))
    {
        LX_C3_WARN("PHY: Failed to read vertex counts (name='{}')", name);
        return false;
    }

    const uint32_t totalVertices = normalVertexCount + alphaVertexCount;
    outMesh.vertices.resize(totalVertices);

    if (!isPhy4)
    {
        outMesh.morphTargets.resize(3);
        for (auto& morph : outMesh.morphTargets)
        {
            morph.positions.resize(totalVertices);
        }

        for (uint32_t i = 0; i < totalVertices; ++i)
        {
            PhyVertexFile v{};
            if (!reader.Read(v))
            {
                LX_C3_WARN("PHY: Failed to read vertex {}/{} (name='{}')", i + 1, totalVertices, name);
                return false;
            }

            Vertex& outVertex             = outMesh.vertices[i];
            outVertex.position            = v.positions[0];
            outVertex.uv                  = {v.u, v.v};
            outVertex.hasNormal           = false;
            outVertex.skinning.indices[0] = v.boneIndices[0];
            outVertex.skinning.indices[1] = v.boneIndices[1];
            outVertex.skinning.weights[0] = v.boneWeights[0];
            outVertex.skinning.weights[1] = v.boneWeights[1];

            outMesh.morphTargets[0].positions[i] = v.positions[1];
            outMesh.morphTargets[1].positions[i] = v.positions[2];
            outMesh.morphTargets[2].positions[i] = v.positions[3];
        }
    }
    else
    {
        for (uint32_t i = 0; i < totalVertices; ++i)
        {
            Phy4VertexFile v{};
            if (!reader.Read(v))
            {
                LX_C3_WARN("PHY4: Failed to read vertex {}/{} (name='{}')", i + 1, totalVertices, name);
                return false;
            }

            Vertex& outVertex             = outMesh.vertices[i];
            outVertex.position            = v.position;
            outVertex.uv                  = {v.u, v.v};
            outVertex.hasNormal           = false;
            outVertex.skinning.indices[0] = v.boneIndices[0];
            outVertex.skinning.indices[1] = v.boneIndices[1];
            outVertex.skinning.weights[0] = v.boneWeights[0];
            outVertex.skinning.weights[1] = v.boneWeights[1];
        }
    }

    uint32_t normalTriCount = 0;
    uint32_t alphaTriCount  = 0;
    if (!reader.Read(normalTriCount) || !reader.Read(alphaTriCount))
    {
        LX_C3_WARN("PHY: Failed to read triangle counts (name='{}')", name);
        return false;
    }

    const uint32_t        totalIndices = (normalTriCount + alphaTriCount) * 3;
    std::vector<uint16_t> indices16;
    if (!reader.ReadArray(indices16, totalIndices))
    {
        LX_C3_WARN("PHY: Failed to read indices (count={}, name='{}')", totalIndices, name);
        return false;
    }

    outMesh.indices.resize(indices16.size());
    for (size_t i = 0; i < indices16.size(); ++i)
    {
        outMesh.indices[i] = indices16[i];
    }

    return true;
}

void C3PhyParser::ValidateBoneInfluences(MeshResource& mesh, uint32_t boneCount)
{
    if (boneCount == 0)
    {
        return;
    }

    size_t invalidCount = 0;
    for (auto& vertex : mesh.vertices)
    {
        for (size_t i = 0; i < vertex.skinning.indices.size(); ++i)
        {
            if (vertex.skinning.indices[i] >= boneCount && vertex.skinning.weights[i] > 0.0f)
            {
                vertex.skinning.weights[i] = 0.0f;
                ++invalidCount;
            }
        }
    }

    if (invalidCount > 0)
    {
        LX_C3_WARN("PHY: Zeroed {} out-of-range bone influences (boneCount={})", invalidCount, boneCount);
    }
}

} // namespace LongXi
