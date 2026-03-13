#include "Assets/C3/C3RuntimeLoader.h"

#include <string>

#include "Assets/C3/C3ChunkDispatcher.h"
#include "Assets/C3/C3Container.h"
#include "Assets/C3/C3Log.h"

namespace LXEngine
{

namespace
{

std::string ResolvePath(const C3LoadRequest& request, bool& usedFileId)
{
    usedFileId = false;
    if (!request.virtualPath.empty())
    {
        return request.virtualPath;
    }

    if (request.fileId.has_value())
    {
        usedFileId = true;
        return std::to_string(request.fileId.value());
    }

    return {};
}

} // namespace

bool C3RuntimeLoader::LoadFromVfs(CVirtualFileSystem& vfs, const C3LoadRequest& request, C3LoadResult& outResult)
{
    outResult = C3LoadResult{};

    bool        usedFileId = false;
    std::string path       = ResolvePath(request, usedFileId);
    if (path.empty())
    {
        outResult.error = "Missing virtual path or file ID";
        outResult.warnings.push_back(outResult.error);
        LX_C3_ERROR("Runtime loader missing path/fileId");
        return false;
    }

    std::vector<uint8_t> buffer = vfs.ReadAll(path);
    if (buffer.empty())
    {
        outResult.error = "Failed to read C3 asset from VFS";
        outResult.warnings.push_back(outResult.error);
        if (usedFileId)
        {
            LX_C3_WARN("Runtime loader failed to read fileId {} via VFS", path);
        }
        else
        {
            LX_C3_WARN("Runtime loader failed to read path '{}' via VFS", path);
        }
        return false;
    }

    const bool parsed = LoadFromBuffer(buffer, outResult);
    if (!parsed)
    {
        if (usedFileId)
        {
            LX_C3_WARN("Runtime loader failed to parse fileId {}", path);
        }
        else
        {
            LX_C3_WARN("Runtime loader failed to parse path '{}'", path);
        }
        return false;
    }

    if (!request.requestedTypes.empty())
    {
        if (!request.Wants(C3ResourceType::Mesh))
        {
            outResult.meshes.clear();
        }
        if (!request.Wants(C3ResourceType::Skeleton))
        {
            outResult.skeletons.clear();
        }
        if (!request.Wants(C3ResourceType::Animation))
        {
            outResult.animations.clear();
        }
        if (!request.Wants(C3ResourceType::Particle))
        {
            outResult.particles.clear();
        }
        if (!request.Wants(C3ResourceType::Camera))
        {
            outResult.cameras.clear();
        }
    }

    return true;
}

bool C3RuntimeLoader::LoadFromBuffer(const std::vector<uint8_t>& data, C3LoadResult& outResult)
{
    outResult = C3LoadResult{};

    C3Container container;
    if (!container.LoadFromMemory(data))
    {
        outResult.error = "Invalid C3 container";
        outResult.warnings.push_back(outResult.error);
        LX_C3_WARN("Runtime loader: invalid C3 container");
        return false;
    }

    C3ChunkDispatcher         dispatcher;
    C3ChunkDispatcher::Result parsed;
    if (!dispatcher.Dispatch(container, parsed))
    {
        outResult.error = "Failed to dispatch C3 chunks";
        outResult.warnings.push_back(outResult.error);
        LX_C3_WARN("Runtime loader: dispatch failed");
        return false;
    }

    outResult.meshes        = std::move(parsed.meshes);
    outResult.skeletons     = std::move(parsed.skeletons);
    outResult.animations    = std::move(parsed.animations);
    outResult.particles     = std::move(parsed.particles);
    outResult.cameras       = std::move(parsed.cameras);
    outResult.unknownChunks = std::move(parsed.unknownChunks);
    outResult.success       = true;

    LX_C3_INFO("Runtime load complete (meshes={}, skeletons={}, animations={}, particles={}, cameras={}, unknown={})",
               outResult.meshes.size(),
               outResult.skeletons.size(),
               outResult.animations.size(),
               outResult.particles.size(),
               outResult.cameras.size(),
               outResult.unknownChunks.size());

    if (outResult.meshes.empty() && outResult.skeletons.empty() && outResult.animations.empty() && outResult.particles.empty() &&
        outResult.cameras.empty())
    {
        outResult.warnings.push_back("C3 container contained no recognized chunks");
        LX_C3_WARN("Runtime loader: no recognized chunks");
    }

    return true;
}

} // namespace LXEngine
