#include "Resource/C3ChunkDispatcher.h"

#include "Resource/C3CameParser.h"
#include "Resource/C3Log.h"
#include "Resource/C3MotiParser.h"
#include "Resource/C3PhyParser.h"
#include "Resource/C3PtclParser.h"
#include "Resource/C3Types.h"

namespace LongXi
{

bool C3ChunkDispatcher::Dispatch(const C3Container& container, Result& outResult)
{
    outResult = Result{};

    const auto& chunks = container.GetRawChunks();
    for (const auto& chunk : chunks)
    {
        const FourCC type = chunk.info.type;
        if (type == C3ChunkPhy || type == C3ChunkPhy4)
        {
            MeshResource mesh;
            const bool   isPhy4 = (type == C3ChunkPhy4);
            if (C3PhyParser::Parse(chunk.data, mesh, isPhy4))
            {
                outResult.meshes.push_back(std::move(mesh));
            }
            else
            {
                LX_C3_WARN("Failed to parse PHY chunk");
            }
            continue;
        }

        if (type == C3ChunkMoti)
        {
            SkeletonResource      skeleton;
            AnimationClipResource clip;
            if (C3MotiParser::Parse(chunk.data, skeleton, clip))
            {
                outResult.skeletons.push_back(std::move(skeleton));
                outResult.animations.push_back(std::move(clip));
            }
            else
            {
                LX_C3_WARN("Failed to parse MOTI chunk");
            }
            continue;
        }

        if (type == C3ChunkPtcl || type == C3ChunkPtcl3)
        {
            ParticleResource particle;
            const bool       isPtcl3 = (type == C3ChunkPtcl3);
            if (C3PtclParser::Parse(chunk.data, particle, isPtcl3))
            {
                outResult.particles.push_back(std::move(particle));
            }
            else
            {
                LX_C3_WARN("Failed to parse PTCL chunk");
            }
            continue;
        }

        if (type == C3ChunkCame)
        {
            CameraPathResource camera;
            if (C3CameParser::Parse(chunk.data, camera))
            {
                outResult.cameras.push_back(std::move(camera));
            }
            else
            {
                LX_C3_WARN("Failed to parse CAME chunk");
            }
            continue;
        }

        // Preserve unknown chunk data
        outResult.unknownChunks.push_back(chunk);
        LX_C3_WARN("Unknown chunk type '{}' (size={})", FourCCToString(type), chunk.info.size);
    }

    uint32_t maxBoneCount = 0;
    for (const auto& skeleton : outResult.skeletons)
    {
        if (skeleton.boneCount > maxBoneCount)
        {
            maxBoneCount = skeleton.boneCount;
        }
    }

    if (maxBoneCount > 0)
    {
        for (auto& mesh : outResult.meshes)
        {
            C3PhyParser::ValidateBoneInfluences(mesh, maxBoneCount);
        }
    }

    return true;
}

} // namespace LongXi
