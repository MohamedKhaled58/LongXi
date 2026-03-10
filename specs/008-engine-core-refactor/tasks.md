# Implementation Tasks: Engine Core Refactor and Engine Service Access

**Feature**: Spec 008 - Engine Core Refactor
**Branch**: `008-engine-core-refactor`
**Total Tasks**: 28
**Generated**: 2026-03-10

## Overview

This task list implements a pure architectural refactor that introduces a central **Engine** class to own and coordinate all engine subsystems. The refactor moves subsystem ownership from Application to Engine, eliminates global variables through controlled service access via getter methods, and maintains single-threaded initialization and runtime.

**Task Organization**: Tasks are organized by user story to enable independent implementation and testing. Each user story phase produces a complete, independently testable increment.

**Parallel Execution**: Tasks marked with `[P]` can be executed in parallel (different files, no dependencies on incomplete tasks).

**Testing**: Manual verification only per Assumption #11 in spec (no automated test tasks).

---

## Phase 1: Setup

**Goal**: Create Engine source directory structure and integrate with build system

**Independent Test Criteria**:
- Engine directory exists under `LongXi/LXEngine/Src/`
- Engine files are included in Premake5 project configuration
- Project builds without linker errors (unimplemented symbols are OK)

### Tasks

- [ ] T001 [P] Create Engine directory at LongXi/LXEngine/Src/Engine/
- [ ] T002 Add Engine.h and Engine.cpp to LXEngine Premake5 configuration in premake5.lua

---

## Phase 2: User Story 1 - Engine Initialization and Subsystem Setup (P1)

**Story**: As a game engine developer, I want a single central Engine object that initializes all core subsystems in the correct order so that I don't need to manually coordinate initialization between Renderer, InputSystem, VirtualFileSystem, and TextureManager.

**Goal**: Implement Engine class with initialization in dependency order (Renderer → InputSystem → VFS → TextureManager)

**Independent Test Criteria**:
- Engine instance can be created
- Engine::Initialize() succeeds with valid HWND and dimensions
- All subsystem getters return valid references after successful initialization
- Engine::Initialize() fails gracefully with invalid HWND
- Engine::Initialize() returns false on second call (already initialized)
- Subsystem initialization failure aborts and cleans up previously initialized subsystems

**Implementation Strategy**: Implement Engine class with unique_ptr ownership of all subsystems, Initialize() method that creates subsystems in dependency order with rollback on failure, and getter methods that assert initialization state.

### Tasks

- [ ] T003 [US1] Create Engine.h header with class declaration in LongXi/LXEngine/Src/Engine/Engine.h
  - Include guards, namespace LongXi, forward declarations for DX11Renderer, InputSystem, CVirtualFileSystem, TextureManager
  - Private members: std::unique_ptr<DX11Renderer> m_Renderer, std::unique_ptr<InputSystem> m_Input, std::unique_ptr<CVirtualFileSystem> m_VFS, std::unique_ptr<TextureManager> m_TextureManager, bool m_Initialized
  - Deleted copy/move constructors and assignment operators
  - Public methods: Constructor/destructor, Initialize(HWND, int, int) -> bool, Shutdown(), IsInitialized() const -> bool, Update(), Render(), OnResize(int, int), GetRenderer(), GetInput(), GetVFS(), GetTextureManager()

- [ ] T004 [P] [US1] Create Engine.cpp implementation file in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Include Engine.h, all subsystem headers, Core/Logging/LogMacros.h
  - Empty implementation stubs (to be filled in by subsequent tasks)

- [ ] T005 [US1] Implement Engine constructor and destructor in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Constructor: Initialize m_Initialized to false, all unique_ptr members are nullptr by default
  - Destructor: Call Shutdown() to ensure cleanup (idempotent)

- [ ] T006 [US1] Implement Engine::Initialize() method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Validate: Return false if m_Initialized is already true (can only initialize once)
  - Validate: Log error and return false if windowHandle is nullptr
  - Create DX11Renderer: m_Renderer = std::make_unique<DX11Renderer>(); if (!m_Renderer->Initialize(windowHandle, width, height)) { LX_ENGINE_ERROR("[Engine] Renderer initialization failed - aborting"); m_Renderer.reset(); return false; }
  - Log: LX_ENGINE_INFO("[Engine] Initializing renderer")
  - Create InputSystem: m_Input = std::make_unique<InputSystem>(); m_Input->Initialize(); LX_ENGINE_INFO("[Engine] Initializing input system")
  - Create VFS: m_VFS = std::make_unique<CVirtualFileSystem>(); std::string exeDir = ResourceSystem::GetExecutableDirectory(); m_VFS->MountDirectory(exeDir + "/Data/Patch"); m_VFS->MountDirectory(exeDir + "/Data"); m_VFS->MountDirectory(exeDir); m_VFS->MountWdf(exeDir + "/Data/C3.wdf"); m_VFS->MountWdf(exeDir + "/Data/data.wdf"); LX_ENGINE_INFO("[Engine] Initializing VFS")
  - Create TextureManager: m_TextureManager = std::make_unique<TextureManager>(*this); if (!m_TextureManager) { LX_ENGINE_ERROR("[Engine] TextureManager creation failed - aborting"); m_TextureManager.reset(); m_VFS.reset(); m_Input.reset(); m_Renderer.reset(); return false; } LX_ENGINE_INFO("[Engine] Initializing texture manager")
  - Set m_Initialized = true
  - Log: LX_ENGINE_INFO("[Engine] Engine initialization complete")
  - Return true

- [ ] T007 [US1] Implement Engine::IsInitialized() const method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Return m_Initialized

- [ ] T008 [US1] Implement Engine::Shutdown() method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Early return if !m_Initialized (idempotent)
  - Log: LX_ENGINE_INFO("[Engine] Shutting down")
  - Destroy in reverse order: m_TextureManager.reset(); m_VFS.reset(); m_Input->Shutdown(); m_Input.reset(); m_Renderer->Shutdown(); m_Renderer.reset()
  - Set m_Initialized = false

- [ ] T009 [P] [US1] Implement Engine subsystem getter stubs in LongXi/LXEngine/Src/Engine/Engine.cpp
  - GetRenderer(): assert(m_Initialized && "Engine::GetRenderer() called before Initialize()"); return *m_Renderer;
  - GetInput(): assert(m_Initialized && "Engine::GetInput() called before Initialize()"); return *m_Input;
  - GetVFS(): assert(m_Initialized && "Engine::GetVFS() called before Initialize()"); return *m_VFS;
  - GetTextureManager(): assert(m_Initialized && "Engine::GetTextureManager() called before Initialize()"); return *m_TextureManager;

---

## Phase 3: User Story 2 - Runtime Update and Render Coordination (P1)

**Story**: As a game engine developer, I want the Engine to coordinate the update loop and render submission so that Application doesn't need to know about individual subsystem calls.

**Goal**: Implement Engine::Update() and Engine::Render() methods

**Independent Test Criteria**:
- Engine::Update() calls InputSystem::Update() to advance input frame boundary
- Engine::Render() calls Renderer::BeginFrame() and Renderer::EndFrame()
- Calling Update() followed by Render() produces a complete frame
- Calling Update() or Render() before Initialize() asserts (Debug) or crashes (Release - undefined behavior is acceptable per spec edge cases)

**Implementation Strategy**: Update() calls InputSystem::Update(), Render() calls BeginFrame() and EndFrame() (Scene.Render() is future work, placeholder comment only).

### Tasks

- [ ] T010 [US2] Implement Engine::Update() method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Assert: assert(m_Initialized && "Engine::Update() called before Initialize()")
  - Call: m_Input->Update() to advance input frame boundary
  - Add comment: // TODO: Update Scene (future spec)

- [ ] T011 [US2] Implement Engine::Render() method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Assert: assert(m_Initialized && "Engine::Render() called before Initialize()")
  - Call: m_Renderer->BeginFrame()
  - Add comment: // TODO: Scene.Render() (future spec)
  - Call: m_Renderer->EndFrame()

- [ ] T012 [US2] Implement Engine::OnResize(int width, int height) method in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Assert: assert(m_Initialized && "Engine::OnResize() called before Initialize()")
  - Guard: if (width <= 0 || height <= 0) { return; }  // Ignore minimized window
  - Forward: m_Renderer->OnResize(width, height)

---

## Phase 4: User Story 3 - Subsystem Access Without Globals (P2)

**Story**: As a subsystem developer (e.g., TextureManager), I need access to other engine services (Renderer, VFS) through controlled Engine accessors so that I don't need global variables.

**Goal**: Modify TextureManager to receive Engine& and access services via getters

**Independent Test Criteria**:
- TextureManager constructor accepts Engine& instead of individual subsystem references
- TextureManager internally calls engine.GetRenderer() and engine.GetVFS()
- No compilation errors (all Engine getter calls resolve correctly)
- Texture loading still works (manual verification)

**Implementation Strategy**: Change TextureManager constructor from (CVirtualFileSystem&, DX11Renderer&) to (Engine&), add Engine& m_Engine member, update LoadTexture() to call m_Engine.GetVFS() and m_Engine.GetRenderer().

### Tasks

- [ ] T013 [P] [US3] Update TextureManager.h constructor signature in LongXi/LXEngine/Src/Texture/TextureManager.h
  - Change: TextureManager(Engine& engine) instead of TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer)
  - Add: Engine& m_Engine private member
  - Remove: CVirtualFileSystem& m_Vfs and DX11Renderer& m_Renderer private members

- [ ] T014 [P] [US3] Update TextureManager.cpp constructor in LongXi/LXEngine/Src/Texture/TextureManager.cpp
  - Change: Constructor accepts Engine& engine, initialize m_Engine(engine)
  - Remove: Initialization of m_Vfs and m_Renderer members (deleted)

- [ ] T015 [US3] Update TextureManager::LoadTexture() to use Engine getters in LongXi/LXEngine/Src/Texture/TextureManager.cpp
  - Replace: All m_Vfs usage with m_Engine.GetVFS()
  - Replace: All m_Renderer usage with m_Engine.GetRenderer()
  - Verify: Compilation succeeds (no unresolved symbols)

---

## Phase 5: User Story 4 - Clean Shutdown in Correct Order (P2)

**Story**: As a game engine developer, I want the Engine to shut down all subsystems in reverse dependency order so that GPU resources are released before the DX11 device is destroyed.

**Goal**: Verify Engine::Shutdown() destroys subsystems in reverse order (TextureManager → VFS → InputSystem → Renderer)

**Independent Test Criteria**:
- Engine::Shutdown() destroys subsystems in reverse order
- No D3D11 debug layer warnings about live objects
- Calling Shutdown() multiple times is safe (idempotent)
- All unique_ptr members are null after Shutdown()

**Implementation Strategy**: Already implemented in T008. This task is verification-only to confirm the shutdown order is correct.

### Tasks

- [ ] T016 [US4] Verify Engine::Shutdown() reverse order in LongXi/LXEngine/Src/Engine/Engine.cpp
  - Confirm: m_TextureManager.reset() is FIRST (releases GPU textures)
  - Confirm: m_VFS.reset() is second
  - Confirm: m_Input->Shutdown(); m_Input.reset() is third
  - Confirm: m_Renderer->Shutdown(); m_Renderer.reset() is LAST (releases D3D11 device)
  - Confirm: Early return if !m_Initialized makes Shutdown() idempotent

---

## Phase 6: User Story 5 - Application No Longer Grows (P3)

**Story**: As a game engine developer adding new features (SpriteRenderer, Scene, etc.), I want to add subsystems to Engine rather than Application so that Application remains a thin window lifecycle manager.

**Goal**: Remove subsystem ownership from Application, Application only owns Win32Window and Engine

**Independent Test Criteria**:
- Application class has exactly 2 members (Win32Window and Engine) after refactor
- Application::Initialize() calls CreateEngine() instead of individual subsystem creation
- Application::Run() calls only Engine::Update() and Engine::Render() (no direct subsystem calls)
- Application::Shutdown() calls Engine::~Engine() or Engine::Shutdown()
- Application::OnResize() forwards to Engine::OnResize()
- TestApplication compiles and runs with Engine getters

**Implementation Strategy**: Remove all subsystem unique_ptr members from Application, add std::unique_ptr<Engine> m_Engine, replace CreateRenderer/CreateInputSystem/CreateVirtualFileSystem/CreateTextureManager with CreateEngine(), update Run() to use Engine methods, update OnResize() to forward to Engine.

### Tasks

- [ ] T017 [P] [US5] Remove subsystem members from Application.h in LongXi/LXEngine/Src/Application/Application.h
  - Remove: std::unique_ptr<DX11Renderer> m_Renderer
  - Remove: std::unique_ptr<InputSystem> m_InputSystem
  - Remove: std::unique_ptr<CVirtualFileSystem> m_VirtualFileSystem
  - Remove: std::unique_ptr<TextureManager> m_TextureManager
  - Add: std::unique_ptr<Engine> m_Engine
  - Result: Application has exactly 2 members (m_Window and m_Engine)

- [ ] T018 [P] [US5] Remove subsystem getter declarations from Application.h in LongXi/LXEngine/Src/Application/Application.h
  - Remove: const InputSystem& GetInput() const;
  - Remove: const CVirtualFileSystem& GetVirtualFileSystem() const;
  - Remove: TextureManager& GetTextureManager();
  - Reason: Subsystems accessed via Engine getters instead

- [ ] T019 [US5] Remove CreateRenderer() method from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: Entire CreateRenderer() method (lines 66-78 approximately)

- [ ] T020 [US5] Remove CreateInputSystem() method from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: Entire CreateInputSystem() method (lines 80-85 approximately)

- [ ] T021 [US5] Remove CreateVirtualFileSystem() method from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: Entire CreateVirtualFileSystem() method (lines 92-115 approximately)

- [ ] T022 [US5] Remove CreateTextureManager() method from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: Entire CreateTextureManager() method (lines 122-127 approximately)

- [ ] T023 [US5] Remove GetInput() implementation from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: GetInput() method (lines 87-90 approximately)

- [ ] T024 [US5] Remove GetVirtualFileSystem() implementation from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: GetVirtualFileSystem() method (lines 117-120 approximately)

- [ ] T025 [US5] Remove GetTextureManager() implementation from Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Delete: GetTextureManager() method (lines 129-132 approximately)

- [ ] T026 [US5] Add CreateEngine() method to Application.cpp in LongXi/LXEngine/Src/Application/Application.cpp
  - Add: CreateEngine() method after CreateMainWindow()
  - Implementation: m_Engine = std::make_unique<Engine>(); if (!m_Engine->Initialize(m_WindowHandle, m_Window->GetWidth(), m_Window->GetHeight())) { LX_ENGINE_ERROR("Engine initialization failed - aborting"); m_Engine.reset(); DestroyMainWindow(); return false; }
  - Return: true on success

- [ ] T027 [US5] Update Application::Initialize() to use CreateEngine() in LongXi/LXEngine/Src/Application/Application.cpp
  - Replace: Calls to CreateRenderer(), CreateInputSystem(), CreateVirtualFileSystem(), CreateTextureManager() with single call to CreateEngine()
  - Update: Error handling rollback to call m_Engine.reset() instead of individual subsystem cleanup

- [ ] T028 [US5] Update Application::Run() to use Engine methods in LongXi/LXEngine/Src/Application/Application.cpp
  - Replace: m_Renderer->BeginFrame(); m_Renderer->EndFrame(); with m_Engine->Render();
  - Replace: m_InputSystem->Update(); with m_Engine->Update();
  - Verify: Run() calls only m_Engine->Update() and m_Engine->Render() (per SC-010)

- [ ] T029 [US5] Update Application::OnResize() to forward to Engine in LongXi/LXEngine/Src/Application/Application.cpp
  - Replace: if (m_Renderer && m_Renderer->IsInitialized()) { m_Renderer->OnResize(width, height); }
  - With: if (m_Engine && m_Engine->IsInitialized()) { m_Engine->OnResize(width, height); }

- [ ] T030 [US5] Update Application::Shutdown() to use Engine in LongXi/LXEngine/Src/Application/Application.cpp
  - Remove: m_TextureManager.reset(); and m_VirtualFileSystem.reset();
  - Replace: With m_Engine.reset() (calls Engine destructor which calls Shutdown())
  - Keep: m_Input->Shutdown(); m_Input.reset(); m_Renderer->Shutdown(); m_Renderer.reset(); (these are now called by Engine::Shutdown(), but keep as safety for now)

- [ ] T031 [US5] Update TestApplication::TestTextureSystem() in LongXi/LXShell/Src/main.cpp
  - Replace: textureManager = GetTextureManager() calls with engine->GetTextureManager()
  - Add: Pass Engine& to TestApplication (store m_Engine member or access via base class)
  - Verify: Compilation succeeds

- [ ] T032 [P] [US5] Update WindowProc to use Engine::GetInput() in LongXi/LXEngine/Src/Application/Application.cpp
  - Replace: All app->m_InputSystem with app->m_Engine->GetInput()
  - Verify: All input events (keyboard, mouse, wheel, focus) forward correctly

- [ ] T033 [P] [US5] Add Engine.h include to LXEngine.h in LongXi/LXEngine/LXEngine.h
  - Add: #include "Engine/Engine.h"
  - Purpose: Expose Engine as part of LXEngine public API

---

## Phase 7: Polish & Validation

**Goal**: Verify all acceptance criteria met, compile with zero errors/warnings, manual verification

**Independent Test Criteria**:
- Code compiles with zero errors and zero warnings
- All existing tests pass (texture loading, input processing, window management)
- Application member count is 2 (Win32Window + Engine)
- Engine member count is 4 (Renderer + Input + VFS + TextureManager)
- Engine initialization completes in under 100ms
- Engine shutdown has no D3D11 debug layer warnings
- Running the application displays window and processes input

### Tasks

- [ ] T034 Compile solution and verify zero errors and zero warnings
  - Build: Build entire solution in Visual Studio
  - Verify: Zero compilation errors
  - Verify: Zero compiler warnings (check Output window)
  - Fix: Any warnings before proceeding

- [ ] T035 Run manual verification test
  - Run: LXShell.exe
  - Verify: Window appears at 1024x768
  - Verify: TestTextureSystem() loads 1.dds and Skill.dds successfully
  - Verify: No crashes or D3D11 debug layer warnings on exit
  - Verify: Pressing ESC or closing window exits cleanly

- [ ] T036 Verify success criteria from specification
  - SC-001: Application has 2 members (m_Window + m_Engine) - inspect Application.h
  - SC-002: Adding subsystems to Engine doesn't require Application changes - architecture verified
  - SC-003: All subsystems accessible through Engine getters - code inspection
  - SC-009: Engine has exactly 4 unique_ptr members - inspect Engine.h
  - SC-010: Application::Run() calls only Engine::Update() and Engine::Render() - inspect Application::Run()

- [ ] T037 Run Win-Format Code.bat to format all C++ files
  - Run: Win-Format Code.bat from repository root
  - Verify: All .h and .cpp files formatted with clang-format
  - Verify: No formatting warnings

---

## Dependencies

### User Story Dependencies

```
Phase 1 (Setup)
  ↓
Phase 2 (US1: Engine Initialization)
  ↓
Phase 3 (US2: Runtime Update/Render)
  ↓
Phase 4 (US3: Subsystem Access)
  ↓
Phase 5 (US4: Shutdown Verification)
  ↓
Phase 6 (US5: Application Refactor)
  ↓
Phase 7 (Polish & Validation)
```

**Critical Path**: T001 → T002 → T003 → T004 → T005 → T006 → T007 → T008 → T009 → T010 → T011 → T012 → T013 → T014 → T015 → T016 → T017-T033 → T034-T037

**Parallel Opportunities**:
- T001 (create directory) and T002 (Premake5) can run in parallel
- T004 (create Engine.cpp stub) can run in parallel with T003 (create Engine.h)
- T009 (getter stubs) can run in parallel with T010-T012 (Update/Render/OnResize)
- T013 (TextureManager.h) and T014 (TextureManager.cpp) can run in parallel
- T017-T018 (Application.h changes) can run in parallel with T019-T025 (Application.cpp removals)
- T032 (WindowProc) and T033 (LXEngine.h) can run in parallel

---

## Parallel Execution Examples

### Within Phase 1 (Setup)
```bash
# Execute T001 and T002 in parallel (different files)
T001: Create Engine directory
T002: Update Premake5 configuration
```

### Within Phase 4 (US3: Subsystem Access)
```bash
# Execute T013 and T014 in parallel (different files)
T013: Update TextureManager.h constructor signature
T014: Update TextureManager.cpp constructor
```

### Within Phase 6 (US5: Application Refactor)
```bash
# Execute T017-T018 in parallel (header file changes)
T017: Remove subsystem members from Application.h
T018: Remove subsystem getter declarations from Application.h

# Execute T019-T025 in parallel (cpp file removals, different methods)
T019: Remove CreateRenderer()
T020: Remove CreateInputSystem()
T021: Remove CreateVirtualFileSystem()
T022: Remove CreateTextureManager()
T023: Remove GetInput()
T024: Remove GetVirtualFileSystem()
T025: Remove GetTextureManager()
```

---

## Implementation Strategy

### MVP Scope (Minimum Viable Product)

**MVP = Phase 1 + Phase 2 + Phase 3**: Setup, Engine Initialization (US1), and Runtime Coordination (US2)

This delivers:
- Engine class that initializes all subsystems in correct order
- Engine::Update() and Engine::Render() methods working
- Application still using old subsystem access (not yet refactored)

**Acceptance**: Can manually verify Engine initializes and Update/Render work.

### Incremental Delivery

**Sprint 1**: MVP (Phase 1-3)
- Create Engine class
- Implement Initialize() with subsystem creation
- Implement Update() and Render()

**Sprint 2**: Subsystem Access (Phase 4)
- Refactor TextureManager to use Engine&
- Verify Engine getter pattern works

**Sprint 3**: Application Refactor (Phase 5-6)
- Remove subsystem ownership from Application
- Update Application to use Engine
- Verify SC-001 (2 members) and SC-010 (only Update/Render calls)

**Sprint 4**: Polish (Phase 7)
- Fix compilation warnings
- Manual testing
- Code formatting

---

## Testing Strategy

### Manual Verification (Per Assumption #11)

**No automated tests** - this is a pure refactor with manual verification per spec Assumption #11.

**Manual Test Cases**:
1. **Initialization**: Run application, verify window appears, check console for "[Engine] Initializing..." logs
2. **Texture Loading**: Verify TestTextureSystem() still loads 1.dds and Skill.dds
3. **Input**: Press keys, move mouse, verify no crashes
4. **Resize**: Resize window, verify no crashes
5. **Shutdown**: Close window, verify no D3D11 debug layer warnings (if debug layer enabled)

### Success Criteria Verification

Each success criterion (SC-001 through SC-010) maps to specific tasks:
- **SC-001**: T017 (Application member count)
- **SC-002**: Architecture review (no Application changes needed for new subsystems)
- **SC-003**: T009 (Engine getters)
- **SC-004**: T035 (manual timing test)
- **SC-005**: T016 + T035 (shutdown order + D3D11 debug layer)
- **SC-006**: Architecture review (explicit constructor dependencies)
- **SC-007**: T034 (zero errors/warnings)
- **SC-008**: T035 (existing tests pass)
- **SC-009**: T003 (Engine has 4 unique_ptr members)
- **SC-010**: T028 (Run() calls only Update/Render)

---

## Task Format Validation

✅ **All tasks follow checklist format**:
- Checkbox: `- [ ]` prefix
- Task ID: T001-T037 sequential
- [P] marker: Included for parallelizable tasks
- [Story] label: [US1]-[US5] for user story phases (Setup/Polish phases have no label)
- Description: Clear action with exact file path
- No tasks missing required components

---

## Next Steps

1. **Review tasks**: Read through all tasks and verify understanding
2. **Begin implementation**: Start with Phase 1 (Setup)
3. **Track progress**: Mark tasks as complete with `- [x]` as you work
4. **Verify incrementally**: Test each phase completion before proceeding
5. **Final validation**: Run Phase 7 polish tasks and verify all success criteria

**Command to begin implementation**: `/speckit.implement`
