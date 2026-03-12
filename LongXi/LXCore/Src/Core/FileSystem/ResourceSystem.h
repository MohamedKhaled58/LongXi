#pragma once

#include <cstdint>
#include <memory>
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
// Archive extension (Spec 005): AddArchive() registers WDF archives that are
// searched after loose-file lookup in ReadFile().
// =============================================================================

namespace LongXi
{

class WdfArchive;

class ResourceSystem
{
public:
    ResourceSystem();  // defined in .cpp — WdfArchive full type required by unique_ptr
    ~ResourceSystem(); // defined in .cpp — WdfArchive full type required by unique_ptr

    ResourceSystem(const ResourceSystem&)            = delete;
    ResourceSystem& operator=(const ResourceSystem&) = delete;

    // Returns the directory containing the current executable as a UTF-8 string.
    static std::string GetExecutableDirectory();

    // Lifecycle — roots are fixed for the session after Initialize()
    void Initialize(const std::vector<std::string>& roots);
    void Shutdown();

    // Existence query — does not open the file
    bool Exists(const std::string& relativePath) const;

    // Whole-file read — checks loose files first, then registered archives.
    // Returns nullopt on any failure (already logged).
    std::optional<std::vector<uint8_t>> ReadFile(const std::string& relativePath) const;

    // Register a WDF archive by relative path (resolved against existing roots).
    bool AddArchive(const std::string& relPath);

private:
    std::string Normalize(const std::string& path) const;
    std::string Resolve(const std::string& normalizedPath) const;

private:
    std::vector<std::string>                 m_Roots;
    std::vector<std::unique_ptr<WdfArchive>> m_Archives;
};

} // namespace LongXi
