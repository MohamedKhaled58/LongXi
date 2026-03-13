#include "Core/StringUtils.h"

#include <algorithm>
#include <cctype>

namespace LXCore
{

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool EndsWithInsensitive(const std::string& value, std::string_view suffix)
{
    if (value.size() < suffix.size())
    {
        return false;
    }

    const size_t start = value.size() - suffix.size();
    for (size_t index = 0; index < suffix.size(); ++index)
    {
        const unsigned char left  = static_cast<unsigned char>(value[start + index]);
        const unsigned char right = static_cast<unsigned char>(suffix[index]);
        if (std::tolower(left) != std::tolower(right))
        {
            return false;
        }
    }

    return true;
}

} // namespace LXCore
