#pragma once

#include "Resource/C3Container.h"
#include "Resource/Resources.h"

namespace LongXi
{

class C3ChunkDispatcher
{
public:
    struct Result
    {
        std::vector<MeshResource>          meshes;
        std::vector<SkeletonResource>      skeletons;
        std::vector<AnimationClipResource> animations;
        std::vector<ParticleResource>      particles;
        std::vector<CameraPathResource>    cameras;
        std::vector<C3Container::RawChunk> unknownChunks;
    };

    bool Dispatch(const C3Container& container, Result& outResult);
};

} // namespace LongXi
