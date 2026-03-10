# Quickstart: Engine Core Refactor

**Feature**: Spec 008 - Engine Core Refactor
**Audience**: Engine developers adding subsystems, application developers integrating Engine
**Date**: 2026-03-10

## What Changed?

### Before the Refactor

Application owned all subsystems directly:

```cpp
class Application {
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_InputSystem;
    std::unique_ptr<CVirtualFileSystem> m_VirtualFileSystem;
    std::unique_ptr<TextureManager> m_TextureManager;
};
```

**Problems**:
- Application class grows with every new subsystem
- Subsystems accessed through Application getters (`GetTextureManager()`)
- No centralized coordination for initialization order
- Global variables or Application access for subsystem-to-subsystem communication

### After the Refactor

Application owns only Engine. Engine owns all subsystems:

```cpp
class Application {
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Engine> m_Engine;  // One member instead of four
};

class Engine {
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_Input;
    std::unique_ptr<CVirtualFileSystem> m_VFS;
    std::unique_ptr<TextureManager> m_TextureManager;
};
```

**Benefits**:
- Application stays thin (6 members → 2 members)
- Subsystems accessed through Engine getters
- Centralized initialization coordination
- Subsystems access other services via Engine reference (no globals)

## For Application Developers

### Basic Usage

```cpp
#include <LXEngine.h>
#include <Application/EntryPoint.h>

namespace LongXi {

class MyApplication : public Application {
  protected:
    bool Initialize() override {
        // Call base class to create window and engine
        if (!Application::Initialize()) {
            return false;
        }

        // Do your custom initialization here
        // All subsystems are ready via m_Engine
        return true;
    }

    void Run() override {
        // Base class already provides the main loop
        // It calls m_Engine->Update() and m_Engine->Render()
        Application::Run();
    }
};

} // namespace LongXi

LongXi::Application* CreateApplication() {
    return new LongXi::MyApplication();
}
```

### Accessing Subsystems

```cpp
class MyApplication : public Application {
  private:
    void LoadGameTextures() {
        // Access TextureManager through Engine
        auto& textureManager = m_Engine->GetTextureManager();

        auto grass = textureManager.LoadTexture("textures/grass.dds");
        auto water = textureManager.LoadTexture("textures/water.dds");
    }

    void CheckInput() {
        // Access InputSystem through Engine
        auto& input = m_Engine->GetInput();

        if (input.IsKeyDown(VK_ESCAPE)) {
            // Quit game
        }

        if (input.WasKeyPressed(VK_SPACE)) {
            // Jump
        }
    }
};
```

### Handling Window Resize

```cpp
// Application::OnResize() now forwards to Engine
void Application::OnResize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;  // Guard against minimized window
    }

    // Forward to Engine
    if (m_Engine && m_Engine->IsInitialized()) {
        m_Engine->OnResize(width, height);
    }
}
```

### WindowProc Integration

```cpp
LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_KEYDOWN:
        if (app && app->m_Engine) {
            auto& input = app->m_Engine->GetInput();
            UINT vk = static_cast<UINT>(wParam);
            bool isRepeat = (lParam & 0x40000000) != 0;
            input.OnKeyDown(vk, isRepeat);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (app && app->m_Engine) {
            auto& input = app->m_Engine->GetInput();
            input.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    // ... other input messages ...
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
```

## For Subsystem Developers

### Creating a New Subsystem

Let's say you want to add a **SceneManager** subsystem to Engine.

#### Step 1: Define the Subsystem Class

```cpp
// LongXi/LXEngine/Src/Scene/SceneManager.h
#pragma once

namespace LongXi {

class Engine;  // Forward declaration

class SceneManager {
  public:
    explicit SceneManager(Engine& engine);
    ~SceneManager();

    // Non-copyable
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // Lifecycle
    bool Initialize();
    void Shutdown();
    void Update();
    void Render();

  private:
    Engine& m_Engine;
    bool m_Initialized;
};

} // namespace LongXi
```

#### Step 2: Implement the Subsystem

```cpp
// LongXi/LXEngine/Src/Scene/SceneManager.cpp
#include "Scene/SceneManager.h"
#include "Engine/Engine.h"  // For Engine::GetRenderer(), etc.

namespace LongXi {

SceneManager::SceneManager(Engine& engine)
    : m_Engine(engine), m_Initialized(false)
{
}

SceneManager::~SceneManager() {
    if (m_Initialized) {
        Shutdown();
    }
}

bool SceneManager::Initialize() {
    LX_ENGINE_INFO("[SceneManager] Initializing");

    // Access other subsystems via Engine getters
    auto& renderer = m_Engine.GetRenderer();
    auto& textureManager = m_Engine.GetTextureManager();

    // ... do initialization ...

    m_Initialized = true;
    LX_ENGINE_INFO("[SceneManager] Initialized successfully");
    return true;
}

void SceneManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    LX_ENGINE_INFO("[SceneManager] Shutting down");

    // ... do cleanup ...

    m_Initialized = false;
}

void SceneManager::Update() {
    if (!m_Initialized) {
        return;
    }

    // ... update scene logic ...
}

void SceneManager::Render() {
    if (!m_Initialized) {
        return;
    }

    // Access renderer via Engine
    auto& renderer = m_Engine.GetRenderer();

    // ... render scene ...
}

} // namespace LongXi
```

#### Step 3: Add to Engine Class

```cpp
// LongXi/LXEngine/Src/Engine/Engine.h
#pragma once
#include "Scene/SceneManager.h"  // Include new subsystem

namespace LongXi {

class Engine {
    // ... existing public interface ...

    // Add new getter
    SceneManager& GetSceneManager();

  private:
    // ... existing subsystems ...

    // Add new subsystem member
    std::unique_ptr<SceneManager> m_SceneManager;
};

} // namespace LongXi
```

#### Step 4: Update Engine Implementation

```cpp
// LongXi/LXEngine/Src/Engine/Engine.cpp
bool Engine::Initialize(HWND windowHandle, int width, int height) {
    // ... existing subsystem initialization ...

    // Initialize SceneManager after TextureManager
    m_SceneManager = std::make_unique<SceneManager>(*this);
    if (!m_SceneManager->Initialize()) {
        LX_ENGINE_ERROR("[Engine] SceneManager initialization failed - aborting");
        m_SceneManager.reset();
        m_TextureManager.reset();
        m_VFS.reset();
        m_Input.reset();
        m_Renderer.reset();
        m_Initialized = false;
        return false;
    }

    LX_ENGINE_INFO("[Engine] SceneManager initialized");
    // ... continue ...
}

void Engine::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Shutdown in REVERSE order (SceneManager first)
    if (m_SceneManager) {
        m_SceneManager.reset();
    }

    // ... rest of shutdown ...
}

void Engine::Update() {
    if (!m_Initialized) {
        return;
    }

    // Update InputSystem
    m_Input->Update();

    // Update SceneManager (new)
    m_SceneManager->Update();
}

void Engine::Render() {
    if (!m_Initialized) {
        return;
    }

    m_Renderer->BeginFrame();

    // Render SceneManager (new)
    m_SceneManager->Render();

    m_Renderer->EndFrame();
}

SceneManager& Engine::GetSceneManager() {
    // Assert or ensure initialized
    return *m_SceneManager;
}
```

#### Step 5: Update LXEngine Public API

```cpp
// LongXi/LXEngine/LXEngine.h
#pragma once

// ... existing includes ...

#include "Scene/SceneManager.h"  // Expose new subsystem
```

**That's it!** You added a new subsystem without touching Application class.

### Subsystem Dependency Rules

#### Good Example (Allowed)

```cpp
// SceneManager depends on Renderer and TextureManager
class SceneManager {
  private:
    Engine& m_Engine;  // Access services via Engine

    void Render() {
        auto& renderer = m_Engine.GetRenderer();     // ✓ Allowed
        auto& texManager = m_Engine.GetTextureManager(); // ✓ Allowed
    }
};
```

#### Bad Example (Forbidden)

```cpp
// SceneManager stores direct references to subsystems
class SceneManager {
  private:
    DX11Renderer& m_Renderer;  // ✗ Bad (bypasses Engine)
    TextureManager& m_TextureManager;  // ✗ Bad (bypasses Engine)
};

// Problem: If Engine ownership changes, SceneManager breaks
// Problem: Cannot validate initialization state
// Problem: Circumvents controlled access pattern
```

#### Bad Example (Forbidden - Globals)

```cpp
// Global subsystem accessor
namespace {
    DX11Renderer* g_Renderer = nullptr;  // ✗ Violates constitution
}

// Problem: Hidden global state
// Problem: Not thread-safe for future MT
// Problem: Violates Article IV of constitution
```

## Migration Checklist

### For Existing Code

#### TextureManager Constructor Changed

**Before**:
```cpp
TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer);
```

**After**:
```cpp
TextureManager(Engine& engine);
```

**Migration**:
```cpp
// Before
auto textureManager = std::make_unique<TextureManager>(vfs, renderer);

// After
auto textureManager = std::make_unique<TextureManager>(engine);
```

#### Application Getters Removed

**Before**:
```cpp
auto& texManager = application.GetTextureManager();
auto& input = application.GetInput();
auto& vfs = application.GetVirtualFileSystem();
```

**After**:
```cpp
auto& texManager = engine.GetTextureManager();
auto& input = engine.GetInput();
auto& vfs = engine.GetVFS();
```

#### Application::Run() Simplified

**Before**:
```cpp
while (true) {
    // ... message loop ...
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
    m_InputSystem->Update();
}
```

**After**:
```cpp
while (true) {
    // ... message loop ...
    m_Engine->Update();  // Includes InputSystem::Update()
    m_Engine->Render();  // Includes BeginFrame/EndFrame
}
```

## Common Patterns

### Initialization Sequence

```cpp
Engine engine;

// Step 1: Create window (Application)
Win32Window window("LongXi", 1024, 768);
window.Create(WindowProc);
HWND hwnd = window.GetHandle();

// Step 2: Initialize Engine
if (!engine.Initialize(hwnd, 1024, 768)) {
    LX_ENGINE_ERROR("Engine initialization failed");
    return false;
}

// Step 3: Custom initialization
auto& texManager = engine.GetTextureManager();
auto playerSprite = texManager.LoadTexture("player.dds");

// Step 4: Main loop
while (running) {
    engine.Update();
    engine.Render();
}

// Step 5: Shutdown (automatic or explicit)
engine.Shutdown();
```

### Subsystem-to-Subsystem Communication

```cpp
class TextureManager {
  public:
    TextureManager(Engine& engine) : m_Engine(engine) {}

    std::shared_ptr<Texture> LoadTexture(const std::string& path) {
        // Access VFS through Engine
        auto& vfs = m_Engine.GetVFS();

        // Load file from VFS
        std::vector<uint8_t> data;
        if (!vfs.LoadFile(path, data)) {
            return nullptr;
        }

        // Decode texture
        auto texData = TextureLoader::LoadDDS(data);

        // Access Renderer through Engine
        auto& renderer = m_Engine.GetRenderer();
        auto gpuTexture = renderer.CreateTexture(
            texData.Width, texData.Height, texData.Format, texData.Pixels.data()
        );

        return std::make_shared<Texture>(gpuTexture, texData.Width, texData.Height, texData.Format);
    }

  private:
    Engine& m_Engine;
};
```

### Error Handling

```cpp
Engine engine;
if (!engine.Initialize(hwnd, 1024, 768)) {
    // Option 1: Log and exit
    LX_ENGINE_ERROR("Failed to initialize engine");
    return 1;

    // Option 2: Retry with different parameters (not recommended for Engine)
    // Option 3: Graceful degradation (run without engine - not recommended)
}

// Always check IsInitialized() before using getters
if (engine.IsInitialized()) {
    auto& input = engine.GetInput();  // Safe
}
```

## Troubleshooting

### Problem: Getter Crashes with "Access Violation"

**Cause**: Calling getter before Engine::Initialize() succeeds

**Solution**:
```cpp
// WRONG
Engine engine;
auto& renderer = engine.GetRenderer();  // 💥 Crashes

// CORRECT
Engine engine;
if (engine.Initialize(hwnd, 1024, 768)) {
    auto& renderer = engine.GetRenderer();  // ✓ Safe
}
```

### Problem: Compilation Error "undefined symbol 'GetRenderer'"

**Cause**: Missing `#include "Engine/Engine.h"` or LXEngine.h

**Solution**:
```cpp
// WRONG
#include "Renderer/DX11Renderer.h"  // Not enough

// CORRECT
#include "Engine/Engine.h"  // Pulls in all subsystem headers

// OR (recommended)
#include <LXEngine.h>  // Public API includes Engine
```

### Problem: Linker Error "unresolved external symbol"

**Cause**: Forgot to add Engine.cpp to Premake5 project

**Solution**: Ensure Engine.cpp is included in LXEngine files:
```lua
-- In premake5.lua
files {
    "LXEngine/Src/Engine/**.h",
    "LXEngine/Src/Engine/**.cpp",
}
```

### Problem: D3D11 Debug Layer Reports Live Objects

**Cause**: Subsystems not shut down in reverse order

**Solution**: Ensure Engine::Shutdown() destroys in reverse order:
```cpp
void Engine::Shutdown() {
    // TextureManager FIRST (releases GPU textures)
    m_TextureManager.reset();

    // Then VFS
    m_VFS.reset();

    // Then InputSystem
    m_Input.reset();

    // Then Renderer (releases D3D11 device)
    m_Renderer.reset();
}
```

## Performance Tips

### Getter Calls Are Free

```cpp
// These calls are inlined and optimized away:
auto& renderer = engine.GetRenderer();  // No overhead
auto& input = engine.GetInput();        // No overhead
```

### Store References Locally (Optional)

```cpp
// If you call getters many times in a loop, store reference locally:
void MySystem::Update() {
    auto& renderer = m_Engine.GetRenderer();  // Get once

    for (int i = 0; i < 1000; i++) {
        renderer.DoSomething();  // Use cached reference
    }
}
```

### Engine Creation is One-Time

```cpp
// CORRECT: Create once, use forever
Engine engine;
engine.Initialize(hwnd, 1024, 768);
while (running) {
    engine.Update();
    engine.Render();
}

// WRONG: Don't recreate Engine every frame
// (huge performance cost, memory leaks)
```

## Next Steps

1. **Read the full spec**: `specs/008-engine-core-refactor/spec.md`
2. **Study the data model**: `specs/008-engine-core-refactor/data-model.md`
3. **Review the API contract**: `specs/008-engine-core-refactor/contracts/engine-api.md`
4. **Implement**: Run `/speckit.tasks` to generate implementation tasks

## Summary

**Key Takeaways**:

- ✅ Application now owns only Engine (not individual subsystems)
- ✅ Engine owns all subsystems (Renderer, Input, VFS, TextureManager)
- ✅ Subsystems accessed via Engine getters (`GetRenderer()`, `GetInput()`, etc.)
- ✅ Subsystems receive `Engine&` to access other services
- ✅ No global variables (constitutional compliance)
- ✅ Easy to add new subsystems without changing Application

**Migration Path**:

1. Update `TextureManager` constructor to accept `Engine&`
2. Update `Application` to own `Engine` instead of individual subsystems
3. Update `Application::Run()` to call `Engine::Update()` and `Engine::Render()`
4. Update `TestApplication` to use `m_Engine->GetXXX()` getters
5. Verify compilation and manual testing

**Result**: Cleaner architecture, easier to extend, no global state.
