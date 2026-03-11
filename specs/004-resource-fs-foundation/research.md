# Research: Resource and Filesystem Foundation

**Branch**: `004-resource-fs-foundation`
**Purpose**: Resolve Win32 file I/O and path handling decisions before implementation

---

## R1: std::fstream Binary Read with Wide Paths on MSVC

**Decision**: Use `std::ifstream` constructed with `std::wstring` for file open on MSVC, read into `std::vector<uint8_t>` using `tellg`/`seekg` pattern.

**Rationale**: MSVC's `std::ifstream` constructor accepts `const wchar_t*` and `std::wstring` as a Microsoft extension (also standardized in C++23). This avoids raw Win32 handle management (`CreateFileW`/`ReadFile`/`CloseHandle`) while still correctly handling Unicode paths. Binary mode (`std::ios::binary`) must be specified to prevent newline translation.

**Implementation pattern**:
```cpp
std::optional<std::vector<uint8_t>> ReadFileBytes(const std::wstring& absolutePath)
{
    std::ifstream file(absolutePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return std::nullopt;

    auto size = static_cast<std::streamsize>(file.tellg());
    if (size <= 0)
        return std::vector<uint8_t>{}; // zero-byte file is valid, return empty buffer

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        return std::nullopt;

    return buffer;
}
```

**Pitfalls**:
- Omitting `std::ios::binary` causes `\r\n` → `\n` translation on Windows, corrupting binary asset data
- `std::ios::ate` opens at end-of-file, allowing `tellg()` to give file size before rewinding
- Zero-byte files are valid — return an empty `std::vector<uint8_t>{}`, not `std::nullopt`

**Alternatives considered**: Raw `CreateFileW`/`ReadFile`/`CloseHandle`. Rejected: more verbose, more error paths, no benefit for a single-threaded bootstrap implementation.

---

## R2: File Existence Check — GetFileAttributesW vs std::filesystem::exists

**Decision**: Use `std::filesystem::exists` and `std::filesystem::is_regular_file` (C++17, available in MSVC) for existence queries. Pass a `std::filesystem::path` constructed from a `std::wstring`.

**Rationale**: `std::filesystem` is available since C++17 and is fully supported on MSVC. It handles the exists/is-file distinction cleanly without manual `INVALID_FILE_ATTRIBUTES` checks. It also avoids conflicting with the project's `ReadFile` macro name from `<windows.h>` (Win32 defines `ReadFile` as a macro, which conflicts with `std::ifstream::read` only if the macro is in scope — best to minimize Win32 header inclusion in implementation).

**Implementation pattern**:
```cpp
bool FileExists(const std::wstring& absolutePath)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::is_regular_file(absolutePath, ec);
}
```

**Alternatives considered**: `GetFileAttributesW` + `INVALID_FILE_ATTRIBUTES` check. Still valid and slightly faster, but `std::filesystem` is cleaner for this use case and is already in the C++ standard.

---

## R3: Path Traversal Prevention

**Decision**: Reject any normalized path that contains a `..` segment. Log a warning and return failure.

**Rationale**: Path traversal sequences (`../`, `..\\`) allow callers to escape the configured resource root and access arbitrary filesystem locations. This is a correctness and security invariant that must be enforced at the normalization step, before any root concatenation.

**Normalization rules (applied in order)**:
1. Trim leading and trailing ASCII whitespace
2. Replace all `\` with `/`
3. Collapse consecutive `/` into a single `/`
4. Strip leading `/` (make path relative)
5. **Reject** if any path segment equals `..` after splitting on `/` → return `""` and log a warning
6. Strip trailing `/`

**Implementation**: After normalization, split on `/` and check each segment:
```cpp
for (const auto& segment : segments)
{
    if (segment == "..")
    {
        LX_CORE_WARN("Rejected path traversal: {}", originalPath);
        return "";
    }
}
```

Single-dot segments (`.`) are collapsed (treated as no-op), not rejected.

---

## R4: GetModuleFileNameW for Executable Directory

**Decision**: Use `GetModuleFileNameW(nullptr, buffer, MAX_PATH)` to get the full executable path, then strip the filename to get the directory.

**Implementation pattern**:
```cpp
std::wstring GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path().wstring();
}
```

**Rationale**: `GetModuleFileNameW(nullptr, ...)` returns the path of the current process executable. `std::filesystem::path::parent_path()` cleanly strips the filename. This is the standard pattern for Windows apps that need to locate files relative to their install location.

**Note**: For the bootstrap, `Application::CreateResourceSystem()` will call this and register `exe_dir + "/Data"` as the default resource root if it exists, or fall back to `exe_dir` directly.

---

## R5: std::string → std::wstring Conversion

**Decision**: Use `MultiByteToWideChar(CP_UTF8, ...)` for `std::string` (UTF-8) → `std::wstring` conversion at Win32 OS call boundaries.

**Rationale**: `std::wstring_convert` is deprecated in C++17 and removed in C++26. The correct Windows approach is `MultiByteToWideChar` with `CP_UTF8`. Since all internal resource paths are ASCII-compatible for Long Xi assets, `CP_ACP` would also work, but `CP_UTF8` is more correct and forward-compatible.

**Implementation pattern**:
```cpp
std::wstring ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
    return result;
}
```

This function is file-scope static in `ResourceSystem.cpp` — it does not appear in the public header.

---

## R6: std::optional<std::vector<uint8_t>> Return Type

**Decision**: Use `std::optional<std::vector<uint8_t>>` as the return type for `ReadFile`. Return `std::nullopt` on any failure; return an engaged optional (possibly containing an empty vector) on success.

**Rationale**: `std::optional` is available since C++17 and is idiomatic for nullable results without exceptions. It makes the failure path explicit at the call site without requiring out-parameters or error codes. `std::vector<uint8_t>` move semantics ensure no unnecessary copies even for large files.

**Available**: Yes — MSVC supports `std::optional` since VS 2017 / C++17 mode. C++23 mode (as used in this project) fully supports it.

---

## R7: Whole-File Read Size Concern

**Decision**: Whole-file read into `std::vector<uint8_t>` is acceptable for all Long Xi 6609 asset files.

**Rationale**: Long Xi 6609 asset files are in the following approximate size ranges:
- `.ini` config files: 1 KB – 100 KB
- `.c3` character models: 10 KB – 2 MB
- `.dds` textures: 64 KB – 16 MB
- `.scene` files: 100 KB – 5 MB
- `.wdf` archives: 10 MB – 200 MB (but WDF parsing is deferred; individual extracted entries are much smaller)

Files up to ~50 MB read synchronously in under 1 second on modern hardware. For the bootstrap phase (loose files only, no WDF), whole-file reads are entirely appropriate. Stream-oriented access is reserved as a future extension point for archive extraction paths.

---

## R8: std::filesystem vs GetFileAttributesW — Final Decision

**Decision**: Use `std::filesystem` for path existence checks and path manipulation; use `MultiByteToWideChar` + `std::ifstream(wstring)` for file open/read. Do not use raw `CreateFileW`/`ReadFile` in this spec.

**Rationale**: `std::filesystem` (C++17) is fully available on MSVC, handles edge cases cleanly, and keeps the implementation readable. Raw Win32 HANDLE-based I/O adds error-handling complexity with no meaningful performance benefit for a synchronous single-threaded bootstrap. The Win32 `ReadFile` macro name conflict with the function name is also avoided by not including `<windows.h>` in the implementation where possible.

---

## Summary of Key Decisions

| Topic | Decision |
|-------|----------|
| Internal path type | `std::string`, UTF-8/ASCII, forward-slash normalized |
| Wide conversion | `MultiByteToWideChar(CP_UTF8)` at OS boundary only |
| File open/read | `std::ifstream(wstring)` in binary mode |
| Existence check | `std::filesystem::is_regular_file` |
| Traversal prevention | Reject paths containing `..` segments after normalization |
| Executable dir | `GetModuleFileNameW` + `parent_path()` |
| Return type | `std::optional<std::vector<uint8_t>>` |
| Read granularity | Whole-file only; stream reserved as future extension |
| Module placement | `ResourceSystem` in `LXCore/Src/Core/FileSystem/` |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
