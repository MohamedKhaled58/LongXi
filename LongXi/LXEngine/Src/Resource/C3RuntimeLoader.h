#pragma once

#include <vector>

#include "Core/FileSystem/VirtualFileSystem.h"
#include "Resource/C3RuntimeTypes.h"

namespace LongXi
{

class C3RuntimeLoader
{
public:
    bool LoadFromVfs(CVirtualFileSystem& vfs, const C3LoadRequest& request, C3LoadResult& outResult);
    bool LoadFromBuffer(const std::vector<uint8_t>& data, C3LoadResult& outResult);
};

} // namespace LongXi
