# Engine API Contract

**Feature**: Spec 008 - Engine Core Refactor
**Interface Type**: Public C++ API (Internal Engine Subsystem)
**Version**: 1.0.0
**Date**: 2026-03-10

## Overview

The **Engine** class provides a centralized API for managing all engine subsystems (Renderer, InputSystem, VirtualFileSystem, TextureManager). This contract defines the public interface, usage patterns, and behavioral guarantees.

## Contract Type

This is an **internal library API** contract within the LXEngine static library. The Engine class is NOT exposed to external consumers (LXShell uses Application, not Engine directly).

## Public Interface

### Class Definition

```cpp
namespace LongXi {

class Engine
{
  public:
    // =========================================================================
    // Lifecycle
    // =========================================================================

    Engine();
    ~Engine();

    // Non-copyable, non-movable (deleted)
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /// Initialize all engine subsystems in dependency order
    ///
    /// Initialization order:
    ///   1. DX11Renderer (requires window handle)
    ///   2. InputSystem
    ///   3. CVirtualFileSystem
    ///   4. TextureManager (requires Renderer + VFS)
    ///
    /// @param windowHandle Valid Win32 window handle (HWND)
    /// @param width Window client area width in pixels
    /// @param height Window client area height in pixels
    /// @return true if all subsystems initialized successfully, false on any failure
    ///
    /// @pre Window must be created and valid
    /// @pre Engine must not already be initialized (Initialize can only succeed once)
    /// @post If successful, all subsystem getters return valid references
    /// @post If failed, Engine is in uninitialized state (safe to destroy or retry)
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    /// @side_effects Creates all subsystems, logs progress with [Engine] prefix
    /// @error_handling Returns false and cleans up previously initialized subsystems
    bool Initialize(HWND windowHandle, int width, int height);

    /// Shutdown all engine subsystems in reverse dependency order
    ///
    /// Shutdown order:
    ///   1. TextureManager (releases GPU textures)
    ///   2. CVirtualFileSystem
    ///   3. InputSystem
    ///   4. DX11Renderer (releases D3D11 device)
    ///
    /// @pre None (safe to call on initialized or uninitialized Engine)
    /// @post All subsystems destroyed, Engine is in uninitialized state
    /// @post Can be called again (idempotent)
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    /// @side_effects Destroys all subsystems, resets m_Initialized flag
    void Shutdown();

    /// Check if Engine is initialized
    ///
    /// @return true if Initialize() succeeded and Shutdown() has not been called
    ///
    /// @pre None
    /// @post None (const query)
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    bool IsInitialized() const;

    // =========================================================================
    // Runtime Loop
    // =========================================================================

    /// Advance one frame of engine runtime (input, game logic, etc.)
    ///
    /// Called once per frame before Render().
    ///
    /// Current responsibilities:
    ///   - Call InputSystem::Update() to advance input frame boundary
    ///
    /// Future responsibilities (not in this spec):
    ///   - Update Scene
    ///   - Update game systems
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post Input frame boundary advanced (Pressed/Released transitions cleared)
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    /// @side_effects Advances input state, clears frame-specific flags
    void Update();

    /// Render one frame
    ///
    /// Called once per frame after Update().
    ///
    /// Current responsibilities:
    ///   - Call Renderer::BeginFrame()
    ///   - [future: Scene.Render()]
    ///   - Call Renderer::EndFrame()
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post Frame presented to screen
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    /// @side_effects Submits GPU commands, presents swap chain
    void Render();

    /// Handle window resize event
    ///
    /// Called by Application when WM_SIZE received.
    ///
    /// @param width New window client area width in pixels
    /// @param height New window client area height in pixels
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @pre width > 0 && height > 0 (guards against minimized window)
    /// @post Renderer resize target recreated for new dimensions
    ///
    /// @thread_safety Single-threaded only (Phase 1)
    /// @side_effects Calls Renderer::OnResize(), recreates D3D11 render target
    void OnResize(int width, int height);

    // =========================================================================
    // Subsystem Accessors
    // =========================================================================

    /// Get reference to DX11Renderer subsystem
    ///
    /// @return Reference to DX11Renderer (valid for Engine lifetime)
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post None (const object, non-const reference returned)
    ///
    /// @usage_patterns CreateTexture(), GetDevice(), GetContext(), etc.
    /// @thread_safety Single-threaded only (Phase 1)
    DX11Renderer& GetRenderer();

    /// Get reference to InputSystem subsystem
    ///
    /// @return Reference to InputSystem (valid for Engine lifetime)
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post None (const object, non-const reference returned)
    ///
    /// @usage_patterns IsKeyDown(), WasKeyPressed(), GetMousePosition(), etc.
    /// @thread_safety Single-threaded only (Phase 1)
    InputSystem& GetInput();

    /// Get reference to CVirtualFileSystem subsystem
    ///
    /// @return Reference to CVirtualFileSystem (valid for Engine lifetime)
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post None (const object, non-const reference returned)
    ///
    /// @usage_patterns Exists(), GetSize(), LoadFile()
    /// @thread_safety Single-threaded only (Phase 1)
    CVirtualFileSystem& GetVFS();

    /// Get reference to TextureManager subsystem
    ///
    /// @return Reference to TextureManager (valid for Engine lifetime)
    ///
    /// @pre Engine must be initialized (asserts m_Initialized)
    /// @post None (const object, non-const reference returned)
    ///
    /// @usage_patterns LoadTexture(), GetTexture(), ClearCache()
    /// @thread_safety Single-threaded only (Phase 1)
    TextureManager& GetTextureManager();

  private:
    // Subsystem ownership (not part of public API)
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_Input;
    std::unique_ptr<CVirtualFileSystem> m_VFS;
    std::unique_ptr<TextureManager> m_TextureManager;
    bool m_Initialized;
};

} // namespace LongXi
```

## Usage Contracts

### Initialization Pattern

```cpp
// CORRECT: Initialize after window creation
Win32Window window("LongXi", 1024, 768);
window.Create(WindowProc);
HWND hwnd = window.GetHandle();

Engine engine;
if (!engine.Initialize(hwnd, 1024, 768)) {
    // Handle initialization failure
    LX_ENGINE_ERROR("Engine initialization failed");
    return false;
}

// Engine is now ready
LX_ENGINE_INFO("Engine initialized successfully");

// INCORRECT: Initialize with invalid HWND
Engine engine;
engine.Initialize(nullptr, 1024, 768);  // ✗ FAILS (asserts or returns false)

// INCORRECT: Initialize multiple times
Engine engine;
engine.Initialize(hwnd, 1024, 768);   // ✓ SUCCEEDS
engine.Initialize(hwnd, 1024, 768);   // ✗ FAILS (returns false, already initialized)
```

### Subsystem Access Pattern

```cpp
// CORRECT: Access subsystems via Engine getters after initialization
Engine engine;
engine.Initialize(hwnd, 1024, 768);

// Access subsystems for operations
auto texture = engine.GetTextureManager().LoadTexture("1.dds");
if (texture) {
    LX_ENGINE_INFO("Texture loaded: {}x{}", texture->GetWidth(), texture->GetHeight());
}

bool isEscapePressed = engine.GetInput().IsKeyDown(VK_ESCAPE);

// INCORRECT: Access subsystems before initialization
Engine engine;
auto& vfs = engine.GetVFS();  // ✗ ASSERTS (m_Initialized == false)

// INCORRECT: Store subsystem references beyond Engine lifetime
TextureManager* textureMgr = nullptr;
{
    Engine engine;
    engine.Initialize(hwnd, 1024, 768);
    textureMgr = &engine.GetTextureManager();  // ✗ DANGLING POINTER
}  // Engine destroyed here
textureMgr->LoadTexture("1.dds");  // ✗ CRASH (use-after-free)
```

### Runtime Loop Pattern

```cpp
// CORRECT: Update before Render, call every frame
Engine engine;
engine.Initialize(hwnd, 1024, 768);

MSG msg = {};
while (true) {
    // Drain Win32 messages
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Engine frame
    engine.Update();   // Advance input frame boundary
    engine.Render();   // Render and present
}

// INCORRECT: Render before Update
engine.Render();   // ✗ WRONG ORDER (input not advanced)

// INCORRECT: Call Update multiple times per frame
engine.Update();
engine.Update();  // ✗ WRONG (input frame advanced twice, skips transitions)
engine.Render();
```

### Window Resize Pattern

```cpp
// CORRECT: Forward WM_SIZE to Engine::OnResize()
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Engine* engine = reinterpret_cast<Engine*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_SIZE:
        if (engine) {
            int width = static_cast<int>(LOWORD(lParam));
            int height = static_cast<int>(HIWORD(lParam));
            engine->OnResize(width, height);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// INCORRECT: Pass zero or negative dimensions
engine->OnResize(0, 0);        // ✗ IGNORED (guarded internally)
engine->OnResize(-100, 200);   // ✗ IGNORED (guarded internally)
```

### Shutdown Pattern

```cpp
// CORRECT: Explicit shutdown before destruction (optional but clear)
Engine engine;
engine.Initialize(hwnd, 1024, 768);
// ... use engine ...
engine.Shutdown();  // Explicit cleanup

// CORRECT: Rely on destructor (Shutdown called automatically)
Engine engine;
engine.Initialize(hwnd, 1024, 768);
// ... use engine ...
// Destructor calls Shutdown() automatically

// CORRECT: Shutdown is idempotent (safe to call multiple times)
engine.Shutdown();
engine.Shutdown();  // ✓ SAFE (no-op, no crash)

// CORRECT: Shutdown on uninitialized Engine is safe
Engine engine;
engine.Shutdown();  // ✓ SAFE (no-op, no crash)
```

## Behavioral Guarantees

### Initialization Guarantees

1. **Ordering**: Subsystems are initialized in dependency order (Renderer → Input → VFS → TextureManager)
2. **Atomicity**: If any subsystem fails, all previously initialized subsystems are destroyed
3. **Once-only**: Initialize() can only succeed once (subsequent calls return false)
4. **Validation**: HWND is validated before passing to Renderer initialization

### Runtime Guarantees

1. **Frame order**: Update() must be called before Render() each frame
2. **Input advancement**: Update() advances input frame boundary (Pressed/Released cleared)
3. **Rendering**: Render() calls BeginFrame() → [scene] → EndFrame()

### Accessor Guarantees

1. **Non-null**: All getters return valid references when Engine is initialized
2. **Lifetime**: Returned references remain valid for Engine's lifetime
3. **Assert on misuse**: Getters assert if Engine not initialized

### Shutdown Guarantees

1. **Reverse order**: Subsystems destroyed in reverse initialization order
2. **GPU cleanup**: TextureManager releases GPU textures before Renderer shutdown
3. **Idempotent**: Safe to call Shutdown() multiple times
4. **Destructor safety**: Engine destructor calls Shutdown() automatically

## Error Handling

### Initialize() Failure Handling

```cpp
Engine engine;
if (!engine.Initialize(hwnd, 1024, 768)) {
    // Engine is in safe uninitialized state
    // Options:
    // 1. Retry with different parameters
    // 2. Log error and exit application
    // 3. Destroy Engine and continue without it (graceful degradation)

    // All subsystems are cleaned up - no memory leaks
    return false;
}
```

### Subsystem Getter Failure Handling

```cpp
Engine engine;  // Not initialized

// Getters assert if Engine not initialized
// In Debug build: Triggers assertion failure
// In Release build: Undefined behavior (dereference null)

// CORRECT: Always check IsInitialized() or ensure Initialize() succeeded
if (engine.IsInitialized()) {
    auto& texManager = engine.GetTextureManager();  // Safe
}
```

### OnResize() Validation

```cpp
// Engine::OnResize() guards against invalid dimensions internally:
void Engine::OnResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;  // Silent ignore (minimized window)
    }
    m_Renderer->OnResize(width, height);
}

// Callers don't need to validate, but WM_SIZE may pass 0x0 for minimized window
```

## Threading Model

### Phase 1 Contract (Current Specification)

**Guarantee**: All Engine methods are **single-threaded only**.

- Initialize(), Shutdown(), Update(), Render(), OnResize() must be called on the same thread
- All subsystem accessors must be called on the same thread as the above methods
- No internal thread synchronization (no mutexes, no atomics)

**Rationale**: Constitution Article IX mandates Phase 1 single-threaded execution for stability and determinism.

### Future MT Compatibility

**Design for Future**: Reference passing (Engine&) is compatible with future multithreading if:
- Engine lifetime exceeds all subsystem access
- External synchronization coordinates access
- No mutable global state is introduced (Constitution Article IV compliance)

## Performance Contracts

### Initialization Performance

**Target**: <100ms for typical window size (1024x768) per SC-004

**Measured By**: Manual testing with high-resolution timer

**Includes**:
- DX11Renderer initialization (device creation, swap chain, render target)
- InputSystem initialization (key state tables)
- CVirtualFileSystem initialization (mounting directories/archives)
- TextureManager initialization (cache allocation)

### Runtime Performance

**Target**: Zero performance regression vs current implementation per Assumption #12

**Includes**:
- Getter calls: Inline (optimized away by compiler)
- Update() call: Single InputSystem::Update() call (same as current)
- Render() call: Same renderer calls as current Application::Run()
- Reference access: No induration vs current direct member access

## Logging Contract

### Required Logging Messages

All Engine initialization steps must log with `[Engine]` prefix per FR-013:

```
[Engine] Initializing renderer
[Engine] Initializing input system
[Engine] Initializing VFS
[Engine] Initializing texture manager
[Engine] Engine initialization complete
```

### Failure Logging

Initialization failures must log with `[Engine] ERROR` prefix:

```
[Engine] ERROR: Renderer initialization failed - aborting
[Engine] ERROR: VFS initialization failed - aborting
```

## Versioning and Stability

### API Stability

**Guarantee**: Engine public interface is **stable** for Phase 1.

- No breaking changes to public methods without major version bump
- New subsystems may be added (e.g., Scene) without breaking existing code
- Getter methods will be added for new subsystems following same pattern

### Extension Points

**Future Additions** (out of scope for this spec):
- `GetScene()` - Scene subsystem access
- `GetCamera()` - Camera subsystem access
- `GetAudio()` - Audio subsystem access
- `GetPhysics()` - Physics subsystem access

**Pattern**: New subsystems follow same ownership model:
```cpp
// Future pattern (example):
std::unique_ptr<Scene> m_Scene;
Scene& GetScene();  // Asserts m_Initialized
```

## Testing Requirements

### Manual Verification (Per Assumption #11)

This spec requires **manual verification only** (no automated tests):

1. **Initialization Test**: Create Engine, call Initialize(), verify success
2. **Subsystem Access Test**: Call all getters, verify no crashes
3. **Runtime Loop Test**: Call Update() and Render() in loop, verify no crashes
4. **Resize Test**: Call OnResize() with various dimensions, verify no crashes
5. **Shutdown Test**: Call Shutdown(), verify D3D11 debug layer reports no live objects
6. **Failure Test**: Initialize with invalid HWND, verify graceful failure

### Success Criteria (From SC-001 through SC-010)

- Application member count reduced from 6 to 2
- Adding subsystems to Engine doesn't require Application changes
- All subsystems accessible through Engine getters
- Engine initialization <100ms
- Engine shutdown has no D3D11 live object warnings
- Compilation: zero errors, zero warnings
- Existing tests pass (texture loading, input, window)

## Integration Points

### Application Usage

```cpp
class Application {
  private:
    std::unique_ptr<Engine> m_Engine;

  protected:
    virtual bool Initialize() {
        if (!CreateMainWindow()) return false;
        if (!CreateEngine()) return false;  // Calls Engine::Initialize()
        return true;
    }

    int Run() {
        MSG msg = {};
        while (true) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            m_Engine->Update();  // Only Update/Render calls (SC-010)
            m_Engine->Render();
        }
    }
};
```

### Subsystem Usage (TextureManager Example)

```cpp
class TextureManager {
  private:
    Engine& m_Engine;  // Non-owning reference

  public:
    TextureManager(Engine& engine) : m_Engine(engine) {}

    std::shared_ptr<Texture> LoadTexture(const std::string& path) {
        // Access VFS through Engine
        auto& vfs = m_Engine.GetVFS();

        // Access Renderer through Engine
        auto& renderer = m_Engine.GetRenderer();

        // ... load texture ...
    }
};
```

## Non-Goals

**Explicitly OUT of scope** for this API contract:

- Thread-safe initialization or runtime (Phase 1 is single-threaded)
- Dynamic subsystem addition/removal at runtime
- Hot-reloading of subsystems
- Multi-engine support (only one Engine instance per Application)
- Cross-engine resource sharing

## References

- **Specification**: `specs/008-engine-core-refactor/spec.md`
- **Data Model**: `specs/008-engine-core-refactor/data-model.md`
- **Constitution**: `.specify/memory/constitution.md`
- **Existing Code**: `LongXi/LXEngine/Src/Application/Application.h` (before refactor)
