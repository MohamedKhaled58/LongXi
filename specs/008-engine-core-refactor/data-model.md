# Data Model: Engine Core Refactor

**Feature**: Spec 008 - Engine Core Refactor
**Date**: 2026-03-10
**Status**: Complete

## Overview

This refactor introduces the **Engine** class as a central runtime coordinator. The data model defines ownership relationships, subsystem dependencies, and API contracts.

## Key Entities

### Engine

**Purpose**: Central runtime object that owns and coordinates all engine subsystems

**Ownership Model**:
```
Engine (unique_ptr owner)
├── DX11Renderer (unique_ptr)
├── InputSystem (unique_ptr)
├── CVirtualFileSystem (unique_ptr)
└── TextureManager (unique_ptr)
```

**Public Interface**:
```cpp
namespace LongXi {

class Engine
{
  public:
    Engine();
    ~Engine();

    // Non-copyable, non-movable
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Lifecycle
    bool Initialize(HWND windowHandle, int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    // Runtime
    void Update();
    void Render();
    void OnResize(int width, int height);

    // Subsystem accessors (non-const reference)
    DX11Renderer& GetRenderer();
    InputSystem& GetInput();
    CVirtualFileSystem& GetVFS();
    TextureManager& GetTextureManager();

  private:
    // Subsystem ownership
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_Input;
    std::unique_ptr<CVirtualFileSystem> m_VFS;
    std::unique_ptr<TextureManager> m_TextureManager;

    // State
    bool m_Initialized;
};

} // namespace LongXi
```

**State Transitions**:
```
[Uninitialized] → Initialize() → [Initialized] → Shutdown() → [Uninitialized]
                     ↓ returns false          (normal operation)
                  [Uninitialized]
```

**Invariants**:
- If `m_Initialized == true`, all subsystem pointers are non-null
- If `m_Initialized == false`, all subsystem pointers are null
- All subsystem getters assert `m_Initialized` before returning references
- Shutdown can be called safely on both initialized and uninitialized Engine
- Initialize can only be called once (returns false on second call)

---

### DX11Renderer

**Purpose**: Direct3D 11 rendering device, swap chain, and render target management

**Ownership**: Owned by Engine via `std::unique_ptr<DX11Renderer>`

**Existing Interface** (unchanged):
```cpp
class DX11Renderer
{
  public:
    bool Initialize(HWND hwnd, int width, int height);
    void BeginFrame();
    void EndFrame();
    void OnResize(int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    RendererTextureHandle CreateTexture(uint32_t width, uint32_t height,
                                       TextureFormat format, const void* pixels);
};
```

**Dependencies**: None (independent subsystem)

**Accessed By**:
- Engine (direct ownership)
- TextureManager (via Engine::GetRenderer())

---

### InputSystem

**Purpose**: Keyboard and mouse input processing with frame boundary management

**Ownership**: Owned by Engine via `std::unique_ptr<InputSystem>`

**Existing Interface** (unchanged):
```cpp
class InputSystem
{
  public:
    void Initialize();
    void Shutdown();
    void Update();  // Advances frame boundary

    void OnKeyDown(UINT vk, bool isRepeat);
    void OnKeyUp(UINT vk);
    void OnMouseMove(int x, int y);
    void OnMouseButtonDown(MouseButton button);
    void OnMouseButtonUp(MouseButton button);
    void OnMouseWheel(short delta);
    void OnFocusLost();
};
```

**Dependencies**: None (independent subsystem)

**Accessed By**:
- Engine (direct ownership)
- Application (via Engine::GetInput() for WindowProc forwarding)

---

### CVirtualFileSystem

**Purpose**: Virtual filesystem for accessing files from WDF archives and loose directories

**Ownership**: Owned by Engine via `std::unique_ptr<CVirtualFileSystem>`

**Existing Interface** (unchanged):
```cpp
class CVirtualFileSystem
{
  public:
    bool MountDirectory(const std::string& path);
    bool MountWdf(const std::string& wdfPath);

    bool Exists(const std::string& path);
    size_t GetSize(const std::string& path);
    bool LoadFile(const std::string& path, std::vector<uint8_t>& outData);
};
```

**Dependencies**: None (independent subsystem, uses ResourceSystem for executable directory)

**Accessed By**:
- Engine (direct ownership)
- TextureManager (via Engine::GetVFS())

---

### TextureManager

**Purpose**: Texture loading, caching, and GPU resource management

**Ownership**: Owned by Engine via `std::unique_ptr<TextureManager>`

**Modified Interface** (constructor changes):
```cpp
// BEFORE (current state):
TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer);

// AFTER (refactored):
TextureManager(Engine& engine);
```

**Internal Access Pattern** (after refactor):
```cpp
class TextureManager
{
  private:
    Engine& m_Engine;  // Non-owning reference to Engine

    // Convenience accessors (inline)
    DX11Renderer& GetRenderer() { return m_Engine.GetRenderer(); }
    CVirtualFileSystem& GetVFS() { return m_Engine.GetVFS(); }
};
```

**Dependencies**:
- **Engine** (constructor parameter - non-owning reference)
- **Renderer** (accessed via `m_Engine.GetRenderer()`)
- **VFS** (accessed via `m_Engine.GetVFS()`)

**Accessed By**:
- Engine (direct ownership)
- Application (via Engine::GetTextureManager())

---

### Application

**Purpose**: Window lifecycle and Win32 message loop manager

**Ownership**: Owns Win32Window and Engine (changed from owning all subsystems)

**Modified Interface**:
```cpp
class Application
{
  protected:
    // Factory method override for TestApplication
    virtual bool Initialize();

  private:
    bool CreateMainWindow();
    bool CreateEngine();  // NEW - replaces CreateRenderer, CreateInputSystem, etc.

    void OnResize(int width, int height);

  private:
    HWND m_WindowHandle;
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Engine> m_Engine;  // NEW - replaces all subsystem members
    bool m_ShouldShutdown;
    bool m_Initialized;
};
```

**Member Changes**:
```
BEFORE:                              AFTER:
├── std::unique_ptr<Win32Window>     ├── std::unique_ptr<Win32Window>
├── std::unique_ptr<DX11Renderer>    └── std::unique_ptr<Engine>  ⭐ NEW
├── std::unique_ptr<InputSystem>
├── std::unique_ptr<CVirtualFileSystem>
└── std::unique_ptr<TextureManager>
```

**Removals** (deleted methods):
- `CreateRenderer()` - replaced by `CreateEngine()`
- `CreateInputSystem()` - replaced by `CreateEngine()`
- `CreateVirtualFileSystem()` - replaced by `CreateEngine()`
- `CreateTextureManager()` - replaced by `CreateEngine()`
- `GetInput()` const - removed (use `m_Engine->GetInput()`)
- `GetVirtualFileSystem()` const - removed (use `m_Engine->GetVFS()`)
- `GetTextureManager()` - removed (use `m_Engine->GetTextureManager()`)

**Additions**:
- `CreateEngine()` - Creates Engine and calls Engine::Initialize()

---

## Dependency Graph

### Initialization Order

```
1. Application::Initialize()
   ├─ CreateMainWindow() → Win32Window created
   └─ CreateEngine()
       └─ Engine::Initialize()
           ├─ Create DX11Renderer (requires HWND from window)
           ├─ Create InputSystem
           ├─ Create CVirtualFileSystem
           └─ Create TextureManager (requires Renderer + VFS)
```

### Runtime Call Graph

```
Application::Run()
├─ Win32 message loop (PeekMessage/DispatchMessage)
└─ Per-frame:
   ├─ Engine::Update()
   │   └─ InputSystem::Update()
   └─ Engine::Render()
       ├─ DX11Renderer::BeginFrame()
       ├─ [future: Scene.Render()]
       └─ DX11Renderer::EndFrame()
```

### Shutdown Order (reverse of initialization)

```
Application::Shutdown()
└─ Engine::~Engine() or Engine::Shutdown()
   ├─ Destroy TextureManager (releases GPU textures)
   ├─ Destroy CVirtualFileSystem
   ├─ Destroy InputSystem
   └─ Destroy DX11Renderer (releases D3D11 device)
```

---

## Relationships Summary

### Ownership Relationships

| Owner | Owned | Ownership Type | Lifetime |
|-------|-------|----------------|----------|
| Application | Win32Window | `std::unique_ptr` | Application lifetime |
| Application | Engine | `std::unique_ptr` | Application lifetime |
| Engine | DX11Renderer | `std::unique_ptr` | Engine lifetime |
| Engine | InputSystem | `std::unique_ptr` | Engine lifetime |
| Engine | CVirtualFileSystem | `std::unique_ptr` | Engine lifetime |
| Engine | TextureManager | `std::unique_ptr` | Engine lifetime |

### Reference Relationships (Non-owning)

| Holder | Referent | Reference Type | Purpose |
|--------|----------|----------------|---------|
| TextureManager | Engine | `Engine&` | Access Renderer and VFS |

### Access Paths (After Refactor)

```
TestApplication::Initialize()
└─ Application::GetTextureManager()
   └─ Engine::GetTextureManager()
      └─ Returns TextureManager& reference

TestApplication::LoadTextures()
└─ engine.GetTextureManager().LoadTexture("1.dds")
   └─ TextureManager internally calls:
       ├─ m_Engine.GetVFS() to access VFS
       └─ m_Engine.GetRenderer() to access Renderer
```

---

## Validation Rules

### Engine Class

- ✅ `Initialize()` must only succeed once (subsequent calls return false)
- ✅ `Initialize()` aborts and cleans up if any subsystem fails
- ✅ `GetRenderer()` asserts `m_Initialized` before returning
- ✅ `GetInput()` asserts `m_Initialized` before returning
- ✅ `GetVFS()` asserts `m_Initialized` before returning
- ✅ `GetTextureManager()` asserts `m_Initialized` before returning
- ✅ `Shutdown()` is idempotent (safe to call multiple times)
- ✅ `Update()` only valid when `m_Initialized == true`
- ✅ `Render()` only valid when `m_Initialized == true`
- ✅ `OnResize()` validates width/height > 0 before forwarding

### Application

- ✅ `Initialize()` logs `[Engine]` prefix for all Engine initialization steps
- ✅ `Run()` calls only `Engine::Update()` and `Engine::Render()` (per SC-010)
- ✅ `Shutdown()` destroys Engine before Win32Window
- ✅ `OnResize()` guards against zero-area dimensions
- ✅ WindowProc forwards input events to `m_Engine->GetInput()`

### TextureManager

- ✅ Constructor receives `Engine&` (not individual subsystems)
- ✅ Stores `Engine&` as member reference (not unique_ptr/shared_ptr)
- ✅ Accesses Renderer via `m_Engine.GetRenderer()`
- ✅ Accesses VFS via `m_Engine.GetVFS()`
- ✅ No direct DX11Renderer or CVirtualFileSystem references

---

## Migration Path

### Step 1: Create Engine Class
- Add `LongXi/LXEngine/Src/Engine/Engine.h` and `Engine.cpp`
- Implement all methods except CreateTextureManager (next step)

### Step 2: Modify TextureManager Constructor
- Change constructor from `TextureManager(CVirtualFileSystem&, DX11Renderer&)` to `TextureManager(Engine&)`
- Update internal access to use `m_Engine.GetRenderer()` and `m_Engine.GetVFS()`

### Step 3: Update Application
- Remove subsystem members (m_Renderer, m_InputSystem, m_VirtualFileSystem, m_TextureManager)
- Add `std::unique_ptr<Engine> m_Engine`
- Replace `CreateRenderer()`, `CreateInputSystem()`, `CreateVirtualFileSystem()`, `CreateTextureManager()` with `CreateEngine()`
- Remove getter methods (GetInput, GetVirtualFileSystem, GetTextureManager)
- Update `OnResize()` to forward to `m_Engine->OnResize()`
- Update `Run()` to call `m_Engine->Update()` and `m_Engine->Render()`

### Step 4: Update TestApplication
- Change `GetTextureManager()` calls to `m_Engine->GetTextureManager()`
- Update WindowProc input forwarding to use `m_Engine->GetInput()`

### Step 5: Update LXEngine.h
- Add `#include "Engine/Engine.h"` to public API header

---

## Type Definitions

### Smart Pointers

- `std::unique_ptr<T>`: Exclusive ownership (Engine → subsystems, Application → Engine)
- `std::shared_ptr<T>`: Shared ownership (TextureManager → cached Texture objects)
- `T&`: Non-owning reference (TextureManager → Engine)

### COM Objects (DX11)

- `Microsoft::WRL::ComPtr<ID3D11Device>`: RAII wrapper for D3D11 device
- `Microsoft::WRL::ComPtr<ID3D11DeviceContext>`: RAII wrapper for D3D11 context
- `Microsoft::WRL::ComPtr<IDXGISwapChain>`: RAII wrapper for DXGI swap chain
- `Microsoft::WRL::ComPtr<ID3D11RenderTargetView>`: RAII wrapper for RTV
- `Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>`: RAII wrapper for texture SRV

---

## Constraints and Assumptions

### Per Specification (FR-014, FR-019, Assumption #13)

- ✅ Single-threaded initialization and runtime
- ✅ Engine initialization in under 100ms (SC-004)
- ✅ No performance regression vs current implementation (Assumption #12)
- ✅ Window resize may occur during Engine initialization (Assumption #7)
- ✅ Application continues to own Win32 message loop (Assumption #6)

### Per Constitution (Article III, Article IX)

- ✅ Windows-only, Win32 API, DirectX 11
- ✅ Static libraries only (LXCore/LXEngine/LXShell)
- ✅ C++23 standard, MSVC v145
- ✅ Phase 1 single-threaded execution
- ✅ MT-aware architecture (reference passing compatible with future threading)

---

## Success Metrics (from SC-001 through SC-010)

| Metric | Target | Verification Method |
|--------|--------|---------------------|
| Application member count | 2 (Win32Window + Engine) | Code inspection after refactor |
| Adding subsystems to Engine | No Application changes required | Code inspection |
| Subsystem access via getters | All subsystems accessible through Engine | Code inspection + compilation |
| Engine initialization time | <100ms for 1024x768 window | Manual testing |
| Engine shutdown | No D3D11 debug layer warnings | D3D11 debug layer output |
| Subsystem dependencies | Explicit via constructor parameters | Code inspection |
| Compilation | Zero errors, zero warnings | Build output |
| Existing tests | Texture loading, input, window all pass | Manual testing |
| Engine member count | 4 unique_ptr members | Code inspection |
| Application::Run() calls | Only Engine::Update() and Engine::Render() | Code inspection |
