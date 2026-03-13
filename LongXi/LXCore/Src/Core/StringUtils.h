#pragma once

#include <string>
#include <string_view>

namespace LongXi
{

// Returns a copy of value with every ASCII letter lowercased.
std::string ToLowerAscii(std::string value);

// Returns true when value ends with suffix (ASCII case-insensitive comparison).
bool EndsWithInsensitive(const std::string& value, std::string_view suffix);

} // namespace LongXi
