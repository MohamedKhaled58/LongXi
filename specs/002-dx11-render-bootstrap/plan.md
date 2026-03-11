# Implementation Plan: DX11 Render Bootstrap

**Branch**: `002-dx11-render-bootstrap` | **Date**: 2026-03-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-dx11-render-bootstrap/spec.md`

## Summary

Integrate a minimal DirectX 11 renderer bootstrap into the existing LXEngine application lifecycle. A new `DX11Renderer` class in `LXEngine/Src/Renderer/` will own device, device context, swap chain, and render target view. Application::Initialize creates the renderer after the window, Application::Run calls BeginFrame/EndFrame each iteration, Application::Shutdown destroys the renderer. The message pump gains a WM_SIZE handler that delegates to the renderer for resize. Build system adds `d3d11.lib`, `dxgi.lib`, and `dxguid.lib` links.

## Technical Context

**Language/Version**: C++23 (MSVC v145)
**Primary Dependencies**: DirectX 11 (`d3d11.h`, `dxgi.h`), Windows SDK (already configured)
**Storage**: N/A
**Testing**: Manual verification (build, launch, visual inspection, resize, shutdown)
**Target Platform**: Windows x64
**Project Type**: Desktop application (static library architecture)
**Performance Goals**: Stable clear/present at VSync (SyncInterval = 1)
**Constraints**: Single-threaded, no premature abstraction, renderer stays in LXEngine
**Scale/Scope**: Single renderer bootstrap class, ~200-300 lines total new code

## Constitution Check

_GATE: Must pass before Phase 0 research. Re-check after Phase 1 design._

| Article                     | Requirement                                | Status  | Notes                                                        |
| --------------------------- | ------------------------------------------ | ------- | ------------------------------------------------------------ |
| III — Technical Baseline    | DX11, Win32, C++23, static libs, MSVC v145 | ✅ PASS | DX11 is constitutionally mandated for Phase 1                |
| III — Module Structure      | LXShell → LXEngine → LXCore, static libs   | ✅ PASS | Renderer class lives in LXEngine, LXShell unchanged          |
| III — Runtime Model         | Central Application, subsystem-oriented    | ✅ PASS | Application owns renderer lifecycle, renderer is a subsystem |
| IV — Entrypoint Discipline  | Thin entrypoints, delegation only          | ✅ PASS | LXShell stays as CreateApplication() only                    |
| IV — Module Boundaries      | No cross-layer coupling                    | ✅ PASS | DX11Renderer is LXEngine-internal                            |
| IV — Abstraction Discipline | No premature abstraction                   | ✅ PASS | Concrete DX11 class, no renderer interface                   |
| IX — Threading              | Single-threaded Phase 1                    | ✅ PASS | No threading introduced                                      |
| XII — Phase 1 Scope         | DX11 init and frame clear/present included | ✅ PASS | Explicitly listed in Article XII line 356                    |

**Gate Result**: ✅ ALL PASS — proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/002-dx11-render-bootstrap/
├── plan.md              # This file
├── research.md          # Phase 0: DX11 bootstrap patterns
├── data-model.md        # Phase 1: DX11Renderer class design
├── quickstart.md        # Phase 1: Build and run instructions
└── tasks.md             # Phase 2 output (via /speckit.tasks)
```

### Source Code (repository root)

```text
LongXi/
├── LXEngine/
│   └── Src/
│       ├── Application/
│       │   ├── Application.h      [MODIFY] Add renderer member, WM_SIZE handling
│       │   ├── Application.cpp    [MODIFY] Init/frame/shutdown renderer calls
│       │   └── EntryPoint.h       [NO CHANGE]
│       ├── Renderer/
│       │   ├── DX11Renderer.h     [NEW] DX11 bootstrap class header
│       │   └── DX11Renderer.cpp   [NEW] DX11 bootstrap implementation
│       └── Window/
│           ├── Win32Window.h      [MODIFY] Add GetWidth/GetHeight accessors
│           └── Win32Window.cpp    [NO CHANGE]
│   └── premake5.lua               [MODIFY] Add d3d11, dxgi links
├── LXShell/
│   └── Src/
│       └── main.cpp               [NO CHANGE]
└── LXCore/                        [NO CHANGE]
```

**Structure Decision**: New `Renderer/` directory under `LXEngine/Src/` for the `DX11Renderer` class. This keeps renderer code in the engine layer without polluting Application, Window, or Shell.

## Complexity Tracking

> No constitutional violations requiring justification.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
