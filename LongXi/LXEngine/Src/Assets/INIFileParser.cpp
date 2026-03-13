#include "Assets/INIFileParser.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "Core/Logging/LogMacros.h"

namespace LXEngine
{

bool INIFileParser::ParseFile(CVirtualFileSystem& vfs, const std::string& relativePath, MappingTable& outMap)
{
    outMap.clear();

    // Check if file exists first
    if (!vfs.Exists(relativePath))
    {
        LX_ENGINE_WARN("[INI] File not found: {}", relativePath);
        return false;
    }

    // Read file content (empty files are valid - they just have 0 entries)
    std::vector<uint8_t> data = vfs.ReadAll(relativePath);
    std::string          content(data.begin(), data.end());
    std::istringstream   stream(content);
    std::string          line;
    uint32_t             lineNumber = 0;

    while (std::getline(stream, line))
    {
        ++lineNumber;

        // Trim leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines
        if (line.empty())
            continue;

        // Find '=' separator
        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos)
        {
            LX_ENGINE_WARN("[INI] Skipping malformed line {} in {}: no '=' found", lineNumber, relativePath);
            continue;
        }

        // Parse key (ID)
        std::string keyStr   = line.substr(0, equalPos);
        std::string valueStr = line.substr(equalPos + 1);

        // Trim whitespace from key and value
        keyStr.erase(0, keyStr.find_first_not_of(" \t"));
        keyStr.erase(keyStr.find_last_not_of(" \t") + 1);
        valueStr.erase(0, valueStr.find_first_not_of(" \t"));
        valueStr.erase(valueStr.find_last_not_of(" \t") + 1);

        // Validate key is numeric
        if (keyStr.empty() || !std::all_of(keyStr.begin(), keyStr.end(), ::isdigit))
        {
            LX_ENGINE_WARN("[INI] Skipping malformed line {} in {}: invalid key '{}'", lineNumber, relativePath, keyStr);
            continue;
        }

        // Validate value is non-empty
        if (valueStr.empty())
        {
            LX_ENGINE_WARN("[INI] Skipping malformed line {} in {}: empty value", lineNumber, relativePath);
            continue;
        }

        // Parse key as uint64_t to handle large IDs (e.g. WeaponMotion.ini uses 13-digit keys)
        uint64_t id = 0;
        try
        {
            id = std::stoull(keyStr);
        }
        catch (const std::exception&)
        {
            LX_ENGINE_WARN("[INI] Skipping malformed line {} in {}: failed to parse key as number", lineNumber, relativePath);
            continue;
        }

        // Normalize path to lowercase
        std::transform(valueStr.begin(),
                       valueStr.end(),
                       valueStr.begin(),
                       [](unsigned char c)
                       {
                           return std::tolower(c);
                       });

        // Insert into map (duplicate keys: last wins)
        outMap[id] = valueStr;
    }

    LX_ENGINE_INFO("[INI] Loaded {} entries from {}", outMap.size(), relativePath);
    return true;
}

} // namespace LXEngine
