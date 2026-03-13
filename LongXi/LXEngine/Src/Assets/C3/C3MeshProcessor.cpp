#include "Assets/C3/C3MeshProcessor.h"

#include <algorithm>
#include <vector>

namespace LXEngine
{

MeshStats C3MeshProcessor::ComputeMeshStats(const MeshResource& mesh)
{
    MeshStats stats;
    stats.vertexCount = mesh.vertices.size();
    stats.indexCount  = mesh.indices.size();

    uint32_t maxIndex = 0;
    for (const auto& vertex : mesh.vertices)
    {
        for (size_t i = 0; i < vertex.skinning.indices.size(); ++i)
        {
            if (vertex.skinning.weights[i] > 0.0f)
            {
                ++stats.weightedInfluences;
                maxIndex = (std::max)(maxIndex, vertex.skinning.indices[i]);
            }
        }
    }

    if (stats.weightedInfluences == 0)
    {
        return stats;
    }

    stats.maxBoneIndex = maxIndex;

    constexpr uint32_t kMaxTrackedBones = 4096;
    if (maxIndex > kMaxTrackedBones)
    {
        return stats;
    }

    std::vector<bool> used(static_cast<size_t>(maxIndex) + 1u, false);
    for (const auto& vertex : mesh.vertices)
    {
        for (size_t i = 0; i < vertex.skinning.indices.size(); ++i)
        {
            if (vertex.skinning.weights[i] > 0.0f)
            {
                const uint32_t idx = vertex.skinning.indices[i];
                if (idx < used.size() && !used[idx])
                {
                    used[idx] = true;
                    ++stats.uniqueBones;
                }
            }
        }
    }

    return stats;
}

bool C3MeshProcessor::IsBetterMesh(const MeshStats& candidate, const MeshStats& best)
{
    if (candidate.uniqueBones != best.uniqueBones)
    {
        return candidate.uniqueBones > best.uniqueBones;
    }
    if (candidate.maxBoneIndex != best.maxBoneIndex)
    {
        return candidate.maxBoneIndex > best.maxBoneIndex;
    }
    if (candidate.weightedInfluences != best.weightedInfluences)
    {
        return candidate.weightedInfluences > best.weightedInfluences;
    }
    if (candidate.vertexCount != best.vertexCount)
    {
        return candidate.vertexCount > best.vertexCount;
    }
    return candidate.indexCount > best.indexCount;
}

int C3MeshProcessor::SelectBestMeshIndex(const std::vector<MeshResource>& meshes)
{
    if (meshes.empty())
    {
        return -1;
    }

    MeshStats bestStats = ComputeMeshStats(meshes[0]);
    int       bestIndex = 0;
    for (size_t i = 1; i < meshes.size(); ++i)
    {
        MeshStats stats = ComputeMeshStats(meshes[i]);
        if (IsBetterMesh(stats, bestStats))
        {
            bestStats = stats;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

MeshBounds C3MeshProcessor::ComputeBounds(const MeshResource& mesh)
{
    MeshBounds bounds;
    if (mesh.vertices.empty())
    {
        return bounds;
    }

    bounds.min   = mesh.vertices.front().position;
    bounds.max   = mesh.vertices.front().position;
    bounds.valid = true;

    for (const auto& vertex : mesh.vertices)
    {
        bounds.min.x = (std::min)(bounds.min.x, vertex.position.x);
        bounds.min.y = (std::min)(bounds.min.y, vertex.position.y);
        bounds.min.z = (std::min)(bounds.min.z, vertex.position.z);

        bounds.max.x = (std::max)(bounds.max.x, vertex.position.x);
        bounds.max.y = (std::max)(bounds.max.y, vertex.position.y);
        bounds.max.z = (std::max)(bounds.max.z, vertex.position.z);
    }

    return bounds;
}

int C3MeshProcessor::SelectSkeletonIndex(const std::vector<SkeletonResource>& skeletons, uint32_t requiredBones)
{
    return SelectSkeletonIndex(skeletons, requiredBones, 0u);
}

int C3MeshProcessor::SelectSkeletonIndex(const std::vector<SkeletonResource>& skeletons, uint32_t requiredBones, uint32_t preferredBones)
{
    if (skeletons.empty())
    {
        return -1;
    }

    int      bestIndex     = -1;
    uint32_t bestBoneCount = 0;

    if (preferredBones > 0)
    {
        for (size_t i = 0; i < skeletons.size(); ++i)
        {
            const uint32_t boneCount = skeletons[i].boneCount;
            if (boneCount == preferredBones && (requiredBones == 0 || boneCount >= requiredBones))
            {
                return static_cast<int>(i);
            }
        }
    }

    if (requiredBones > 0)
    {
        uint32_t bestDelta = 0;
        for (size_t i = 0; i < skeletons.size(); ++i)
        {
            const uint32_t boneCount = skeletons[i].boneCount;
            if (boneCount >= requiredBones)
            {
                if (preferredBones > 0)
                {
                    const uint32_t delta = (boneCount > preferredBones) ? (boneCount - preferredBones) : (preferredBones - boneCount);
                    if (bestIndex < 0 || delta < bestDelta || (delta == bestDelta && boneCount < bestBoneCount))
                    {
                        bestIndex     = static_cast<int>(i);
                        bestBoneCount = boneCount;
                        bestDelta     = delta;
                    }
                }
                else if (bestIndex < 0 || boneCount < bestBoneCount)
                {
                    bestIndex     = static_cast<int>(i);
                    bestBoneCount = boneCount;
                }
            }
        }
        if (bestIndex >= 0)
        {
            return bestIndex;
        }
    }

    if (preferredBones > 0)
    {
        uint32_t bestDelta = 0;
        for (size_t i = 0; i < skeletons.size(); ++i)
        {
            const uint32_t boneCount = skeletons[i].boneCount;
            const uint32_t delta     = (boneCount > preferredBones) ? (boneCount - preferredBones) : (preferredBones - boneCount);
            if (bestIndex < 0 || delta < bestDelta || (delta == bestDelta && boneCount < bestBoneCount))
            {
                bestIndex     = static_cast<int>(i);
                bestBoneCount = boneCount;
                bestDelta     = delta;
            }
        }

        if (bestIndex >= 0)
        {
            return bestIndex;
        }
    }

    for (size_t i = 0; i < skeletons.size(); ++i)
    {
        const uint32_t boneCount = skeletons[i].boneCount;
        if (boneCount > bestBoneCount)
        {
            bestBoneCount = boneCount;
            bestIndex     = static_cast<int>(i);
        }
    }

    return bestIndex;
}

int C3MeshProcessor::SelectAnimationIndex(const std::vector<AnimationClipResource>& clips, uint32_t boneCount)
{
    if (clips.empty())
    {
        return -1;
    }

    for (size_t i = 0; i < clips.size(); ++i)
    {
        if (clips[i].boneCount == boneCount)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

} // namespace LXEngine
