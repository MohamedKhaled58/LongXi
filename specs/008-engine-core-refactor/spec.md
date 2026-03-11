# Feature Specification: Engine Core Refactor and Engine Service Access

**Feature Branch**: `008-engine-core-refactor`
**Created**: 2026-03-10
**Status**: Draft
**Input**: User description: "Spec 008: Engine Core Refactor and Engine Service Access - Introduce central Engine runtime object for subsystem ownership, initialization coordination, update/render loops, and controlled service access without global variables"

## User Scenarios & Testing

### User Story 1 - Engine Initialization and Subsystem Setup (Priority: P1)

As a game engine developer, I want a single central Engine object that initializes all core subsystems in the correct order so that I don't need to manually coordinate initialization between Renderer, InputSystem, VirtualFileSystem, and TextureManager.

**Why this priority**: This is foundational - without proper Engine initialization, no other engine systems can function. The refactor MUST ensure subsystems are initialized in dependency order.

**Independent Test**: Can be tested by creating an Engine instance with a valid window handle and verifying that all subsystems (Renderer, InputSystem, VFS, TextureManager) initialize successfully without manual intervention.

**Acceptance Scenarios**:

1. **Given** a valid Win32 window handle and dimensions, **When** Engine::Initialize() is called, **Then** all subsystems initialize in dependency order (Renderer → InputSystem → VFS → TextureManager) and return true
2. **Given** an Engine has successfully initialized, **When** checking subsystem availability, **Then** all subsystem accessors return non-null references
3. **Given** a subsystem fails to initialize, **When** Engine::Initialize() encounters the failure, **Then** initialization aborts, previously initialized subsystems are cleaned up, and Engine::Initialize() returns false

---

### User Story 2 - Runtime Update and Render Coordination (Priority: P1)

As a game engine developer, I want the Engine to coordinate the update loop and render submission so that Application doesn't need to know about individual subsystem calls.

**Why this priority**: Core runtime behavior - the engine must update and render every frame. This defines the primary execution loop.

**Independent Test**: Can be tested by calling Engine::Update() and Engine::Render() in a loop and verifying that input is processed and frames are rendered without Application calling subsystems directly.

**Acceptance Scenarios**:

1. **Given** an Engine has been initialized, **When** Engine::Update() is called, **Then** InputSystem::Update() is called to advance the input frame boundary
2. **Given** an Engine has been initialized, **When** Engine::Render() is called, **Then** the render sequence executes (Renderer::BeginFrame() → [future Scene.Render()] → Renderer::EndFrame())
3. **Given** Application is running, **When** the main loop calls Engine::Update() followed by Engine::Render(), **Then** a complete frame is processed and displayed

---

### User Story 3 - Subsystem Access Without Globals (Priority: P2)

As a subsystem developer (e.g., TextureManager), I need access to other engine services (Renderer, VFS) through controlled Engine accessors so that I don't need global variables.

**Why this priority**: Critical for code quality - eliminates global state, makes dependencies explicit, and maintains clean architecture. This is a key architectural improvement.

**Independent Test**: Can be tested by creating a subsystem (TextureManager) that requires Engine services, passing the Engine reference to its constructor, and verifying it can access Renderer and VFS through Engine's getter methods.

**Acceptance Scenarios**:

1. **Given** a subsystem requires access to another subsystem, **When** the subsystem calls Engine::GetRenderer(), **Then** a valid Renderer reference is returned
2. **Given** TextureManager is being constructed, **When** it receives Engine& reference, **Then** it can call engine.GetRenderer() and engine.GetVFS() to access required services
3. **Given** multiple subsystems need access to the same service, **When** they call Engine::GetTextureManager(), **Then** they all receive references to the same TextureManager instance (no duplicate instances)

---

### User Story 4 - Clean Shutdown in Correct Order (Priority: P2)

As a game engine developer, I want the Engine to shut down all subsystems in reverse dependency order so that GPU resources are released before the DX11 device is destroyed.

**Why this priority**: Critical for stability - incorrect shutdown order causes crashes and resource leaks. TextureManager must release GPU textures before Renderer shuts down.

**Independent Test**: Can be tested by calling Engine::Shutdown() and verifying that subsystems are destroyed in reverse initialization order (TextureManager → VFS → InputSystem → Renderer) without crashes or D3D11 debug layer warnings.

**Acceptance Scenarios**:

1. **Given** an Engine has initialized subsystems, **When** Engine::Shutdown() is called, **Then** subsystems are destroyed in reverse dependency order (TextureManager → VFS → InputSystem → Renderer)
2. **Given** the Engine is shutting down, **When** TextureManager releases its GPU resources, **Then** all ID3D11ShaderResourceView objects are released before the DX11 device is destroyed
3. **Given** Engine::Shutdown() completes, **When** checking subsystem state, **Then** all unique_ptr members are null and no subsystem calls are possible

---

### User Story 5 - Application No Longer Grows (Priority: P3)

As a game engine developer adding new features (SpriteRenderer, Scene, etc.), I want to add subsystems to Engine rather than Application so that Application remains a thin window lifecycle manager.

**Why this priority**: Important for long-term maintainability - prevents Application class from becoming bloated. This demonstrates the architectural benefit of the refactor.

**Independent Test**: Can be verified by inspecting Application class member count before and after refactor - Application should have fewer members (only Win32Window and Engine), while Engine owns all subsystems.

**Acceptance Scenarios**:

1. **Given** the Engine refactor is complete, **When** comparing Application::Application() members before refactor, **Then** Application no longer stores unique_ptr to DX11Renderer, InputSystem, CVirtualFileSystem, or TextureManager
2. **Given** Application::Initialize() executes, **When** it creates Engine, **Then** Application only stores std::unique_ptr<Engine> alongside Win32Window
3. **Given** future subsystems are added (e.g., Scene), **When** they are added to Engine, **Then** Application class definition does not change

---

### Edge Cases

- What happens when Engine::Initialize() is called with an invalid HWND?
- What happens when Engine::Initialize() is called multiple times?
- What happens when a subsystem getter (e.g., GetRenderer()) is called before Engine::Initialize()?
- What happens when Engine::Update() or Engine::Render() is called before Engine::Initialize()?
- What happens when window resize (WM_SIZE) occurs during Engine initialization?
- What happens when a subsystem fails to initialize after others have already succeeded?
- What happens if the Engine is destroyed while subsystems are still in use?
- What happens when Engine::OnResize() is called with zero or negative dimensions?

## Requirements

### Functional Requirements

- **FR-001**: The system MUST introduce a central `Engine` class that owns all engine subsystems (Renderer, InputSystem, VirtualFileSystem, TextureManager)
- **FR-002**: The system MUST move subsystem ownership from `Application` to `Engine` - Application must NOT store unique_ptr to Renderer, InputSystem, VFS, or TextureManager after refactor
- **FR-003**: The system MUST provide `Engine::Initialize(HWND windowHandle, int width, int height)` method that initializes all subsystems in dependency order
- **FR-004**: The system MUST initialize subsystems in this order: (1) Renderer, (2) InputSystem, (3) VirtualFileSystem, (4) TextureManager
- **FR-005**: The system MUST provide `Engine::Shutdown()` method that destroys subsystems in reverse dependency order
- **FR-006**: The system MUST shut down subsystems in this order: (1) TextureManager, (2) VirtualFileSystem, (3) InputSystem, (4) Renderer
- **FR-007**: The system MUST provide `Engine::Update()` method that calls `InputSystem::Update()` to advance the input frame boundary
- **FR-008**: The system MUST provide `Engine::Render()` method that coordinates the render sequence
- **FR-009**: The system MUST provide `Engine::OnResize(int width, int height)` method that forwards resize events to Renderer
- **FR-010**: The system MUST provide non-const getter methods for all subsystems: `GetRenderer()`, `GetInput()`, `GetVFS()`, `GetTextureManager()`
- **FR-011**: The system MUST allow subsystems to receive Engine& reference during construction to access other services
- **FR-012**: The system MUST ensure that all subsystem accessors return valid references when Engine is initialized
- **FR-013**: The system MUST log initialization progress with `[Engine]` prefix for all subsystem initialization steps
- **FR-014**: The system MUST maintain single-threaded initialization and runtime (no background threading in this spec)
- **FR-015**: The system MUST ensure that Application only stores Win32Window and Engine - Application must NOT directly own or access Renderer, InputSystem, VFS, or TextureManager after refactor
- **FR-016**: The system MUST provide render sequence in `Engine::Render()` that calls `Renderer::BeginFrame()` before scene rendering and `Renderer::EndFrame()` after scene rendering
- **FR-017**: The system MUST validate that HWND is valid before passing it to Renderer initialization
- **FR-018**: The system MUST abort Engine initialization if any subsystem fails to initialize, cleaning up previously initialized subsystems
- **FR-019**: The system MUST log `[Engine] Engine initialization complete` after all subsystems successfully initialize

### Key Entities

- **Engine**: Central runtime coordinator class that owns and manages all engine subsystems
- **DX11Renderer**: Direct3D 11 rendering device, swap chain, and render target management
- **InputSystem**: Keyboard and mouse input processing with frame boundary management
- **CVirtualFileSystem**: Virtual filesystem for accessing files from WDF archives and loose directories
- **TextureManager**: Texture loading, caching, and GPU resource management
- **Application** (modified): Window lifecycle and Win32 message loop manager, no longer owns subsystems directly

## Success Criteria

### Measurable Outcomes

- **SC-001**: Application class member count is reduced from 6 to 2 (only Win32Window and Engine) after refactor
- **SC-002**: Adding a new subsystem to Engine does NOT require changes to Application class definition
- **SC-003**: All subsystems are accessible through Engine getter methods (no global variables required)
- **SC-004**: Engine initialization completes in under 100ms for typical window size (1024x768)
- **SC-005**: Engine shutdown completes without D3D11 debug layer reporting live object warnings
- **SC-006**: Subsystem dependencies are explicitly visible through constructor parameters (no hidden global state)
- **SC-007**: Code compiles with zero errors and zero warnings after refactor
- **SC-008**: All existing tests pass after refactor (texture loading, input processing, window management)
- **SC-009**: Engine class has exactly 4 unique_ptr members (Renderer, InputSystem, VFS, TextureManager) after refactor
- **SC-010**: Application::Run() main loop calls only `Engine::Update()` and `Engine::Render()` (no direct subsystem calls)

## Out of Scope

The following items are explicitly OUT of scope for this specification:

- **Scene system**: Scene graph, scene management, or scene rendering (deferred to future spec)
- **Sprite rendering**: Sprite batch renderer, sprite shaders, or sprite compositing (deferred to future spec)
- **Mesh system**: 3D mesh rendering, skeletal animation, or mesh management (deferred to future spec)
- **Camera system**: Camera management, camera controllers, or viewport management (deferred to future spec)
- **Particle system**: Particle effects, emitters, or particle rendering (deferred to future spec)
- **Background resource loading**: Multi-threaded asset streaming or async resource loading (deferred to future spec)
- **Multi-threading**: Thread pools, job systems, or parallel subsystem updates (deferred to future spec)
- **ECS or DOTS**: Entity component system or data-oriented technology stack (out of scope)
- **Scripting**: Lua scripting, Python scripting, or any scripting integration (out of scope)
- **Networking**: Multiplayer, networking, or replication (out of scope)
- **Physics**: Physics simulation, collision detection, or physics integration (out of scope)
- **Audio**: Sound, music, or audio system integration (out of scope)
- **UI/HUD**: User interface, HUD system, or UI framework (out of scope)

## Assumptions

1. **DX11Renderer dependency**: The existing DX11Renderer requires a valid HWND and window dimensions for initialization. This is provided by Application after window creation.
2. **Subsystem constructors**: Existing subsystems (TextureManager, InputSystem) already accept references to their dependencies. The Engine will pass itself to allow subsystems to access services via getters.
3. **No parallel initialization**: Subsystems initialize sequentially in dependency order. There is no requirement for parallel subsystem initialization.
4. **Existing logging macros**: The project uses `LX_ENGINE_INFO`, `LX_ENGINE_ERROR` macros. Engine should use these for consistency.
5. **Constitution compliance**: This refactor must follow all rules in the project constitution, including namespace rules (LongXi), RAII patterns, and module boundaries.
6. **Win32 message loop**: Application continues to own the Win32 message pump (PeekMessage/DispatchMessage loop). Engine only owns runtime updates and rendering.
7. **Window resize timing**: WM_SIZE messages may arrive during Engine initialization. Application should buffer or queue these until Engine is ready.
8. **Executable directory**: VirtualFileSystem continues to use `ResourceSystem::GetExecutableDirectory()` to locate Data/ directories and WDF archives.
9. **D3D11 device lifetime**: The D3D11 device must remain valid until all Engine subsystems have shut down. This is ensured by correct shutdown order.
10. **Reference passing**: Subsystems receive Engine& as constructor parameter and store it as a reference (not unique_ptr) to access services via getters.
11. **Testing not required**: This is a pure refactor with no new user-facing features. Manual verification (compilation, runtime testing) is sufficient.
12. **No performance regression**: The refactor should not introduce measurable performance overhead compared to the current implementation.
13. **Single-threaded runtime**: Engine::Update() and Engine::Render() are called on the main thread. No thread synchronization is required.
14. **Future extensibility**: The Engine design must support adding new subsystems (Scene, Camera, etc.) in future specs without changing Application.
15. **No global state**: The refactor explicitly eliminates global variables. All state is owned by Engine or Application.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
