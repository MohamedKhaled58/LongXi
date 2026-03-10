# Quickstart: Resource and Filesystem Foundation

**Branch**: `004-resource-fs-foundation`
**Purpose**: Developer reference — what gets added, how the system initializes, and how later parsers use it.

---

## What Gets Added

Two new source files in `LXCore`:

```
LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.h
LongXi/LXCore/Src/Core/FileSystem/ResourceSystem.cpp
```

Two existing files are modified in `LXEngine`:

```
LongXi/LXEngine/Src/Application/Application.h     ← forward decl, m_ResourceSystem, GetResourceSystem()
LongXi/LXEngine/Src/Application/Application.cpp   ← CreateResourceSystem(), lifecycle integration
```

No changes to `LXShell`, the renderer, the input system, or the build scripts.

---

## Startup Initialization

`Application::CreateResourceSystem()` determines the resource root at startup:

```
1. GetModuleFileNameW → exe directory (e.g., "D:/Game/Build/Debug/")
2. Try registering exe_dir + "Data/" as primary root
3. Register exe_dir as fallback root
4. Call m_ResourceSystem->Initialize(roots)
```

Both roots are logged at INFO level. A root that doesn't exist on disk produces a WARN but does not abort initialization.

For verification (AC-002), place a test file at:
```
[exe_dir]/Data/test.dat
```

---

## Frame Lifecycle

`ResourceSystem` is not called per-frame. It is a stateless query service initialized once. There is no `Update()` method.

```
Application::Initialize():
  1. CreateMainWindow()
  2. CreateRenderer()
  3. CreateInputSystem()
  4. CreateResourceSystem()   ← new

Application::Shutdown():
  1. m_ResourceSystem->Shutdown()  ← new (first)
  2. m_InputSystem->Shutdown()
  3. m_Renderer->Shutdown()
  4. DestroyMainWindow()
```

---

## Query API — How Later Parsers Use It

```cpp
// Access via Application
const ResourceSystem& res = app->GetResourceSystem();

// Check existence without opening
if (res.Exists("textures/hero.dds"))
{
    // file is present
}

// Read entire file as raw bytes
auto data = res.ReadFile("config/game.ini");
if (!data.has_value())
{
    // file missing or unreadable — error already logged
    return;
}
// data.value() is std::vector<uint8_t>
// Pass to a parser that interprets the bytes
ParseIni(data.value());
```

**Key rules for parser code:**
- Always check `has_value()` before using the result
- Do not call Win32 file APIs directly in parser code — use `ResourceSystem` exclusively
- Do not store the returned `data` beyond the parser's scope unless intentionally copying it
- Path separators: pass either `"textures/hero.dds"` or `"textures\\hero.dds"` — normalization handles both

---

## Path Normalization Examples

```cpp
res.ReadFile("textures/hero.dds");        // ✓ clean
res.ReadFile("textures\\hero.dds");       // ✓ normalized → same result
res.ReadFile("  textures/hero.dds  ");   // ✓ whitespace stripped
res.ReadFile("textures/../hero.dds");    // ✗ rejected — traversal logged as WARN
res.ReadFile("missing_file.dat");        // ✗ not found — logged as ERROR, returns nullopt
```

---

## Verification (Manual — AC-002)

1. Create `[exe_dir]/Data/test.dat` with known content, e.g., 4 bytes: `0x48 0x65 0x6C 0x6C` ("Hell")
2. Add temporary diagnostic logging in `Application::Run()` (remove before commit):

```cpp
// Temporary verification — remove before commit
auto data = m_ResourceSystem->ReadFile("test.dat");
if (data.has_value())
    LX_ENGINE_INFO("[Resource] Read {} bytes from test.dat", data->size());
else
    LX_ENGINE_INFO("[Resource] test.dat not found");

auto missing = m_ResourceSystem->ReadFile("does_not_exist.dat");
// Should produce ERROR log: "ResourceSystem: file not found: 'does_not_exist.dat'"
```

3. Expected console output:
```
[LXCore  ] [info]  ResourceSystem: registered root 'D:/Game/Build/Debug/Data'
[LXCore  ] [info]  ResourceSystem: registered root 'D:/Game/Build/Debug'
[LXCore  ] [info]  Resource system initialized (2 root(s))
[LXEngine] [info]  [Resource] Read 4 bytes from test.dat
[LXCore  ] [error] ResourceSystem: file not found: 'does_not_exist.dat'
```

---

## What This Does NOT Do

- No `.wdf` archive reading or extraction
- No asset-format parsing (`.c3`, `.ani`, `.dds`, `.ini`, `.scene`, `.dmap`, `.pul`)
- No caching — every `ReadFile` call opens and reads the file from disk
- No async or background loading
- No hot reload
- No dynamic root registration after `Initialize()`
- No stream-oriented access (reserved for future spec)
- No write access of any kind
