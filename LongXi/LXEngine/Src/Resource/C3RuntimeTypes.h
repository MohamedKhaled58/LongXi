#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Resource/C3Container.h"
#include "Resource/Resources.h"

namespace LongXi
{

enum class C3ResourceType
{
    Mesh = 0,
    Skeleton,
    Animation,
    Particle,
    Camera,
};

struct C3LoadRequest
{
    std::string                 virtualPath;
    std::optional<uint32_t>     fileId;
    std::vector<C3ResourceType> requestedTypes;

    bool Wants(C3ResourceType type) const
    {
        if (requestedTypes.empty())
        {
            return true;
        }
        for (C3ResourceType requested : requestedTypes)
        {
            if (requested == type)
            {
                return true;
            }
        }
        return false;
    }
};

struct C3LoadResult
{
    bool                               success = false;
    std::string                        error;
    std::vector<MeshResource>          meshes;
    std::vector<SkeletonResource>      skeletons;
    std::vector<AnimationClipResource> animations;
    std::vector<ParticleResource>      particles;
    std::vector<CameraPathResource>    cameras;
    std::vector<C3Container::RawChunk> unknownChunks;
    std::vector<std::string>           warnings;
};

} // namespace LongXi
