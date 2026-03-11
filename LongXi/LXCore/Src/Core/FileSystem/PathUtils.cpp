#include "Core/FileSystem/PathUtils.h"

#include <cctype>
#include <vector>

namespace LongXi
{

std::string NormalizeVirtualResourcePath(const std::string& path, bool lowercase)
{
    if (path.empty())
        return {};

    auto start = path.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    auto end = path.find_last_not_of(" \t\r\n");
    std::string s = path.substr(start, end - start + 1);

    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
        if (lowercase)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string collapsed;
    collapsed.reserve(s.size());
    bool lastWasSlash = false;
    for (char c : s)
    {
        if (c == '/')
        {
            if (!lastWasSlash)
                collapsed += c;
            lastWasSlash = true;
        }
        else
        {
            collapsed += c;
            lastWasSlash = false;
        }
    }
    s = std::move(collapsed);

    if (!s.empty() && s.front() == '/')
        s.erase(0, 1);
    if (!s.empty() && s.back() == '/')
        s.pop_back();

    std::vector<std::string> segments;
    std::string segment;
    for (char c : s)
    {
        if (c == '/')
        {
            if (segment == "..")
                return {};
            if (!segment.empty() && segment != ".")
                segments.push_back(segment);
            segment.clear();
        }
        else
        {
            segment += c;
        }
    }

    if (segment == "..")
        return {};
    if (!segment.empty() && segment != ".")
        segments.push_back(segment);

    if (segments.empty())
        return {};

    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i > 0)
            result += '/';
        result += segments[i];
    }

    return result;
}

} // namespace LongXi
