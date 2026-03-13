#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "Core/FileSystem/VirtualFileSystem.h"

namespace LXEngine
{

using LXCore::CVirtualFileSystem;

class INIFileParser
{
public:
    using MappingTable = std::unordered_map<uint64_t, std::string>;

    // Parse INI file from VFS into id→path mapping
    // Returns false if file cannot be read (malformed lines don't fail parsing)
    static bool ParseFile(CVirtualFileSystem& vfs, const std::string& relativePath, MappingTable& outMap);

private:
    INIFileParser()  = delete;
    ~INIFileParser() = delete;
};

} // namespace LXEngine
