#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// =============================================================================
// ResourceSystem — Centralized engine resource/filesystem foundation
//
// Resolves, normalizes, and reads files through a controlled interface.
// Roots are registered at Initialize() and fixed for the session.
// All internal paths use forward-slash UTF-8 strings.
// Win32 wide-string conversion occurs only at OS call sites in the .cpp.
//
// Extension boundary: Resolve() is the integration point for future
// archive-backed sources. The public interface does not change when
// archive support is added in a later spec.
// =============================================================================

namespace LongXi
{

class ResourceSystem
{
  public:
    ResourceSystem() = default;
    ~ResourceSystem() = default;

    // Disable copy
    ResourceSystem(const ResourceSystem&) = delete;
    ResourceSystem& operator=(const ResourceSystem&) = delete;

    // Returns the directory containing the current executable as a UTF-8 string.
    // Used by the Application to build default resource root paths at startup.
    static std::string GetExecutableDirectory();

    // Lifecycle — roots are fixed for the session after Initialize()
    void Initialize(const std::vector<std::string>& roots);
    void Shutdown();

    // Existence query — does not open the file
    bool Exists(const std::string& relativePath) const;

    // Whole-file read — returns nullopt on any failure (already logged)
    // Zero-byte files return an engaged optional with an empty vector.
    std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const;

  private:
    // Normalizes a relative path:
    //   - Trims leading/trailing whitespace
    //   - Replaces '\' with '/'
    //   - Collapses '//' into '/'
    //   - Strips leading '/'
    //   - Rejects '..' segments (returns "" and logs WARN)
    //   - Collapses '.' segments
    //   - Strips trailing '/'
    // Returns "" for empty or rejected input.
    std::string Normalize(const std::string& path) const;

    // Resolves a normalized relative path against registered roots.
    // Returns the first absolute path where a regular file exists, or "".
    // Extension boundary: archive sources slot in here in a later spec.
    std::string Resolve(const std::string& normalizedPath) const;

  private:
    // Registered root directories (absolute, forward-slash normalized).
    // Searched in registration order; first match wins.
    std::vector<std::string> m_Roots;
};

} // namespace LongXi
