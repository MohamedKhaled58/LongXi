#pragma once

#include <string>

namespace LXCore
{

// Normalize a virtual resource path.
// - Trims ASCII whitespace
// - Converts '\' to '/'
// - Optionally lowercases ASCII letters
// - Collapses repeated '/'
// - Removes leading/trailing '/'
// - Collapses '.' segments
// - Rejects '..' traversal segments
// Returns empty string when invalid or empty after normalization.
std::string NormalizeVirtualResourcePath(const std::string& path, bool lowercase);

} // namespace LXCore
