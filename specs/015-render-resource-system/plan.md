# Implementation Plan: Spec 015 - Render Resource System

**Branch**: `015-render-resource-system` | **Date**: 2026-03-11 | **Spec**: `D:\Yamen Development\LongXi\specs\015-render-resource-system\spec.md`
**Input**: Feature specification from `D:\Yamen Development\LongXi\specs\015-render-resource-system\spec.md`

## Summary

Introduce a backend-agnostic Render Resource System that formalizes descriptor-driven creation, opaque handle usage, controlled update/bind operations, and renderer-owned lifetime management for textures, buffers, and shaders. The design keeps all DirectX objects and allocations inside DX11 backend modules and enforces strict engine-side boundary rules established by Spec 014.

## Technical Context

**Language/Version**: C++23 (MSVC v145)  
**Primary Dependencies**: Win32 API, DirectX 11 backend (`d3d11`, `dxgi`, `d3dcompiler`), Premake5, spdlog  
**Storage**: N/A (runtime GPU resources and in-memory renderer resource tables)  
**Testing**: `D:\Yamen Development\LongXi\Win-Build Project.bat`, runtime validation scenarios from quickstart, `D:\Yamen Development\LongXi\Scripts\Audit-RendererBoundaries.ps1`  
**Target Platform**: Windows desktop x64 (Visual Studio 2026 toolchain)  
**Project Type**: Native desktop engine (static libraries + shell executable)  
**Performance Goals**: 60 FPS-class runtime with no resource-system-induced frame stutter under normal workload; deterministic resource operation behavior under 1,000-op stress scenarios per resource class  
**Constraints**: No DirectX headers/types in engine-facing modules; renderer-only ownership of GPU resources; opaque-handle API only; single-threaded runtime discipline in Phase 1  
**Scale/Scope**: Changes across `D:\Yamen Development\LongXi\LongXi\LXEngine\Src\Renderer`, `D:\Yamen Development\LongXi\LongXi\LXEngine\Src\Texture`, and related contract/audit/spec artifacts

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Article III - Technical baseline**: PASS
  - Keeps Windows + Win32 + DirectX11 baseline, static library structure, and dependency direction `LXShell -> LXEngine -> LXCore`.
- **Article IV - Architectural laws**: PASS
  - Enforces renderer boundary ownership of backend resources and prevents cross-module DirectX leakage.
- **Article V - State ownership principles**: PASS
  - Resource system is rendering infrastructure only; no gameplay-truth ownership introduced.
- **Article VII - Specification-first delivery**: PASS
  - Plan, research, data model, contracts, and quickstart remain traceable to spec requirements.
- **Article IX - Runtime discipline**: PASS
  - Maintains single-threaded execution assumptions while keeping future MT-safe ownership boundaries.
- **Article XI - Verification and honesty**: PASS
  - Validation path includes build, runtime scenario checks, and boundary audit outcomes with explicit reporting.

**Gate Result (Pre-Research)**: PASS

## Project Structure

### Documentation (this feature)

```text
D:/Yamen Development/LongXi/specs/015-render-resource-system/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── renderer-resource-api-contract.md
│   └── renderer-resource-lifetime-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
D:/Yamen Development/LongXi/LongXi/
├── LXEngine/
│   └── Src/
│       ├── Renderer/
│       │   ├── Renderer.h
│       │   ├── RendererTypes.h
│       │   ├── RendererFactory.cpp
│       │   └── Backend/
│       │       └── DX11/
│       ├── Texture/
│       └── Engine/
├── LXShell/
└── Scripts/
    └── Audit-RendererBoundaries.ps1
```

**Structure Decision**: Extend existing renderer public contract files (`Renderer.h`, `RendererTypes.h`) and DX11 backend folder for resource management internals while keeping `TextureManager` and other engine systems dependent only on backend-agnostic handles/descriptors.

## Phase 0: Research Focus

- Finalize opaque handle strategy that keeps public types backend-agnostic but allows backend table lookup.
- Define descriptor schema boundaries for texture, buffer, and shader creation to avoid API churn.
- Define strict usage/update policy (`Static`, `Dynamic`, `Staging`) and map/unmap safety semantics.
- Define lifetime model and stale-handle rejection policy for deterministic destroy/shutdown behavior.

## Phase 1: Design Focus

- Model resource entities, descriptors, handle records, and state transitions in `data-model.md`.
- Define renderer resource API contract (create/update/bind/destroy) in `contracts/renderer-resource-api-contract.md`.
- Define lifetime/tracking/shutdown contract in `contracts/renderer-resource-lifetime-contract.md`.
- Define practical validation scenarios and expected outcomes in `quickstart.md`.

## Post-Design Constitution Re-Check

- **Article III baseline**: PASS (no platform/toolchain/module baseline changes).
- **Article IV boundaries**: PASS (backend internals remain isolated; engine-facing API remains opaque).
- **Article VII spec-first**: PASS (artifacts remain spec-traceable and implementation-ready).
- **Article IX runtime discipline**: PASS (design preserves current single-threaded runtime constraints).
- **Article XI verification**: PASS (design includes measurable validation and explicit failure diagnostics).

**Gate Result (Post-Design)**: PASS

## Complexity Tracking

No constitutional violations requiring exception justification.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include texture loading, buffer creation, shader management, renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.

## Implementation Verification Notes

- Renderer resource API and backend modules were integrated according to this plan:
  - Public descriptors/opaque handles in `LongXi/LXEngine/Src/Renderer/RendererTypes.h`
  - Public resource operations in `LongXi/LXEngine/Src/Renderer/Renderer.h`
  - Backend modules under `LongXi/LXEngine/Src/Renderer/Backend/DX11/`
- Validation scripts executed:
  - `Scripts/Audit-RendererBoundaries.ps1 -Json` -> `ok: true`
  - `Scripts/Validate-Spec015-ResourceCreation.ps1 -Json` -> `ok: true`
  - `Scripts/Validate-Spec015-UpdatesAndBinding.ps1 -Json` -> `ok: true`
  - `Scripts/Validate-Spec015-Lifetime.ps1 -Json` -> `ok: true`
- Full build validation (`Win-Build Project.bat`) is currently blocked by a Visual Studio/MSBuild file-tracker error in `LXCore` (`MSB4018` from `Microsoft.CppCommon.targets`), unrelated to renderer code changes.
