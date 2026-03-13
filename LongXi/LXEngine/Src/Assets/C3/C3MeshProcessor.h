#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Assets/C3/Resources.h"

namespace LXEngine
{

struct MeshStats
{
    uint32_t uniqueBones        = 0;
    uint32_t maxBoneIndex       = 0;
    uint32_t weightedInfluences = 0;
    size_t   vertexCount        = 0;
    size_t   indexCount         = 0;
};

struct MeshBounds
{
    LXCore::Vector3 min{0.0f, 0.0f, 0.0f};
    LXCore::Vector3 max{0.0f, 0.0f, 0.0f};
    bool            valid = false;
};

class C3MeshProcessor
{
public:
    static MeshStats ComputeMeshStats(const MeshResource& mesh);
    static bool      IsBetterMesh(const MeshStats& candidate, const MeshStats& best);
    static int       SelectBestMeshIndex(const std::vector<MeshResource>& meshes);

    static MeshBounds ComputeBounds(const MeshResource& mesh);

    static int SelectSkeletonIndex(const std::vector<SkeletonResource>& skeletons, uint32_t requiredBones);
    static int SelectSkeletonIndex(const std::vector<SkeletonResource>& skeletons, uint32_t requiredBones, uint32_t preferredBones);
    static int SelectAnimationIndex(const std::vector<AnimationClipResource>& clips, uint32_t boneCount);
};

} // namespace LXEngine
