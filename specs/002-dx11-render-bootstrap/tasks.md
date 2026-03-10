# Tasks: DX11 Render Bootstrap

**Input**: Design documents from `/specs/002-dx11-render-bootstrap/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: No automated tests requested. Verification is manual (build, launch, visual inspection, resize, shutdown).

**Organization**: Single user story (US1 — Renderer Bootstrap, P1). All tasks serve this story directly.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1)
- Exact file paths included in descriptions

---

## Phase 1: Setup (Build System)

**Purpose**: Add DX11 library dependencies and directory structure before any source code

- [ ] T001 Add `d3d11`, `dxgi`, `dxguid` links to `LongXi/LXEngine/premake5.lua`
- [ ] T002 Regenerate Visual Studio project files via `Win-Generate Project.bat`

**Checkpoint**: LXEngine project now links DX11 libraries. Solution opens and builds without errors (no new source yet).

---

## Phase 2: Foundational (Win32Window Accessors)

**Purpose**: Expose window dimensions needed by the renderer — must complete before US1

**⚠️ CRITICAL**: Renderer initialization requires window width/height from Win32Window

- [ ] T003 Add `GetWidth()` and `GetHeight()` inline accessors to `LongXi/LXEngine/Src/Window/Win32Window.h`

**Checkpoint**: Win32Window exposes width/height. Builds without errors.

---

## Phase 3: User Story 1 — Renderer Bootstrap (Priority: P1) 🎯 MVP

**Goal**: Establish a minimal DX11 render bootstrap that initializes against the existing Win32 window, clears to cornflower blue each frame, presents with VSync, handles resize, and shuts down cleanly.

**Independent Test**: Launch `LXShell.exe`, verify window shows cornflower blue (not black), resize the window, close the window — all without crash or debug layer warnings.

### Implementation for User Story 1

**DX11Renderer class (new files)**:

- [ ] T004 [P] [US1] Create `DX11Renderer` class header with member declarations and method signatures in `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`
- [ ] T005 [US1] Implement `DX11Renderer::Initialize` — device, swap chain, render target creation with debug layer flag; check all HRESULT returns and log errors on failure (FR-009) in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T006 [US1] Implement `DX11Renderer::BeginFrame` — bind render target, clear to cornflower blue in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T007 [US1] Implement `DX11Renderer::EndFrame` — present with VSync (SyncInterval = 1) in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T008 [US1] Implement `DX11Renderer::OnResize` — release render target, ResizeBuffers, recreate render target in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T009 [US1] Implement `DX11Renderer::Shutdown` — release all COM objects in reverse-creation order in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T010 [US1] Implement private helpers `CreateRenderTarget()` and `ReleaseRenderTarget()` in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`
- [ ] T011 [US1] Add renderer initialization, frame, and shutdown log messages using `LX_ENGINE_INFO`/`LX_ENGINE_ERROR` in `LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp`

**Application integration (modified files)**:

- [ ] T012 [US1] Add `#include` for DX11Renderer and `std::unique_ptr<DX11Renderer> m_Renderer` member to `LongXi/LXEngine/Src/Application/Application.h`
- [ ] T013 [US1] Add `CreateRenderer()` private method and `OnResize()` handler declaration to `LongXi/LXEngine/Src/Application/Application.h`
- [ ] T014 [US1] Implement `Application::CreateRenderer()` — instantiate and initialize DX11Renderer with window handle and dimensions in `LongXi/LXEngine/Src/Application/Application.cpp`
- [ ] T015 [US1] Modify `Application::Initialize()` — call `CreateRenderer()` after `CreateMainWindow()` in `LongXi/LXEngine/Src/Application/Application.cpp`
- [ ] T016 [US1] Modify `Application::Run()` — call `m_Renderer->BeginFrame()` and `m_Renderer->EndFrame()` each iteration in `LongXi/LXEngine/Src/Application/Application.cpp`
- [ ] T017 [US1] Modify `Application::Shutdown()` — call `m_Renderer->Shutdown()` before `DestroyMainWindow()` in `LongXi/LXEngine/Src/Application/Application.cpp`
- [ ] T018 [US1] Add `WM_SIZE` handling in `Application::WindowProc()` — extract width/height from lParam, delegate to `OnResize()` in `LongXi/LXEngine/Src/Application/Application.cpp`
- [ ] T019 [US1] Implement `Application::OnResize()` — guard against zero-area (minimized), forward to `m_Renderer->OnResize()` in `LongXi/LXEngine/Src/Application/Application.cpp`

**Checkpoint**: Full renderer bootstrap integrated. Application clears to cornflower blue, presents each frame, handles resize, shuts down cleanly.

---

## Phase 4: Verification & Polish

**Purpose**: Build all configurations, manual verification, documentation

- [ ] T020 Build Debug (x64) — verify 0 errors, 0 warnings across LXCore, LXEngine, LXShell
- [ ] T021 Build Release (x64) — verify 0 errors
- [ ] T022 Build Dist (x64) — verify 0 errors
- [ ] T023 Manual test: launch LXShell.exe (Debug), verify cornflower blue window (not black) and DX11 debug layer output visible in console (NFR-003)
- [ ] T024 Manual test: resize window, verify blue persists at new size without corruption
- [ ] T025 Manual test: minimize and restore window, verify blue returns
- [ ] T026 Manual test: close window, verify clean exit with no debug layer warnings in console
- [ ] T027 Manual test: verify renderer init/shutdown log messages appear in console
- [ ] T028 Verify `LXShell/Src/main.cpp` remains unchanged from Spec 001
- [ ] T029 Run `Win-Format Code.bat` to format all source files
- [ ] T030 Update spec.md status from Draft to Accepted

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — Win32Window accessor needed by renderer
- **User Story 1 (Phase 3)**: Depends on Phase 2 — renderer needs window dimensions and DX11 libs linked
- **Verification (Phase 4)**: Depends on Phase 3 — all code must be written before build/test

### Within User Story 1

```text
T004 (header) ──────────────────────────┐
                                        ├── T005-T011 (implementation, sequential)
T003 (Win32Window accessors) ───────────┘
                                        ├── T012-T013 (Application.h changes)
                                        └── T014-T019 (Application.cpp changes, sequential)
```

### Parallel Opportunities

- T001 (premake) and T003 (Win32Window.h) can run in parallel — different files
- T004 (DX11Renderer.h) and T012-T013 (Application.h) can run in parallel — different files
- T020-T022 (three build configs) are sequential (same build system)
- T023-T028 (manual tests) can run as one session

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T002)
2. Complete Phase 2: Foundational (T003)
3. Complete Phase 3: User Story 1 (T004-T019)
4. **STOP and VALIDATE**: Build and test (T020-T028)
5. Mark spec Accepted (T030)

### Single Developer Execution

All phases are sequential for a single developer. Estimated execution: T001-T019 implementation, T020-T030 verification. Total: 30 tasks.

---

## Notes

- [P] tasks = different files, no dependencies
- [US1] label = maps to the single user story (Renderer Bootstrap)
- No test tasks generated — spec does not request automated tests
- Commit after each logical group (Phase 1, Phase 2, Phase 3, Phase 4)
- All DX11 COM objects use `ComPtr<T>` for RAII (per research.md R6)
- Debug layer enabled via `#ifdef LX_DEBUG` (per research.md R7)
