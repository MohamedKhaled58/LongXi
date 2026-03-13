#pragma once

#include "Assets/C3/C3Container.h"
#include "Assets/C3/Resources.h"

namespace LXEngine
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

} // namespace LXEngine
