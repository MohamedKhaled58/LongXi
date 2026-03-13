#pragma once

#include <vector>

#include "Assets/C3/C3RuntimeTypes.h"
#include "Core/FileSystem/VirtualFileSystem.h"

namespace LXEngine
{

class C3RuntimeLoader
{
public:
    bool LoadFromVfs(CVirtualFileSystem& vfs, const C3LoadRequest& request, C3LoadResult& outResult);
    bool LoadFromBuffer(const std::vector<uint8_t>& data, C3LoadResult& outResult);
};

} // namespace LXEngine
