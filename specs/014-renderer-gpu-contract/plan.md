# Implementation Plan: Spec 014 - Renderer API and Backend Architecture

**Branch**: `014-renderer-gpu-contract` | **Date**: 2026-03-11 | **Spec**: `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\spec.md`
**Input**: Feature specification from `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\spec.md`

## Summary

Establish a backend-agnostic renderer API layer so engine subsystems depend only on `Renderer.h` and `RendererTypes.h`, while DirectX 11 remains fully encapsulated in backend implementation modules. The design enforces deterministic frame lifecycle/state reset behavior, strict boundary rules that prevent DirectX type/header leakage into engine systems, and renderer-owned handling for resize, pass sequencing, and external render integration.

## Technical Context

**Language/Version**: C++23 (MSVC v145)  
**Primary Dependencies**: Win32 API, DirectX 11 (`d3d11`, `dxgi`, `d3dcompiler`) in backend only, spdlog, Premake5  
**Storage**: N/A (runtime renderer state and GPU resources only)  
**Testing**: `Win-Build Project.bat`, runtime validation scenarios in quickstart, `Scripts/Audit-RendererBoundaries.ps1`  
**Target Platform**: Windows desktop (x64, Visual Studio 2026 toolchain)  
**Project Type**: Native desktop engine (static libraries + shell executable)  
**Performance Goals**: Stable 60 FPS-class runtime with deterministic renderer lifecycle; zero lifecycle-order failures in 10,000-frame validation run  
**Constraints**: No DirectX headers/types in engine system public interfaces, renderer is sole GPU-state owner, Phase 1 single-threaded runtime discipline  
**Scale/Scope**: Refactor touches renderer API/backend boundaries and integrations across `LXEngine` (Renderer/Engine/Scene/Sprite/Texture) and `LXShell` (ImGui bridge), plus boundary audit tooling and spec artifacts

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Article III - Technical baseline**: PASS
  - Keeps Windows + Win32 + DirectX11 baseline.
  - Keeps static library architecture and dependency direction (`LXShell -> LXEngine -> LXCore`).
- **Article IV - Architectural laws**: PASS
  - Renderer owns GPU state and backend details; engine modules consume narrow public renderer API only.
  - No cross-layer convenience leaks permitted.
- **Article V - State ownership discipline**: PASS
  - Rendering remains presentation infrastructure and does not own gameplay truth.
- **Article VII - Specification-first delivery**: PASS
  - Plan, research, data model, contracts, and quickstart remain traceable to spec requirements.
- **Article IX - Runtime discipline**: PASS
  - Design remains single-thread runtime compliant while preserving future MT-aware boundaries.
- **Article XI - Verification and honesty**: PASS
  - Validation requirements are explicit (build, runtime, boundary audit); no unverifiable guarantees introduced.

**Gate Result (Pre-Research)**: PASS

## Project Structure

### Documentation (this feature)

```text
D:/Yamen Development/LongXi/specs/014-renderer-gpu-contract/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── renderer-api-contract.md
│   └── external-pass-bridge-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
D:/Yamen Development/LongXi/LongXi/
├── LXEngine/
│   └── Src/
│       ├── Engine/
│       ├── Renderer/
│       │   ├── Renderer.h
│       │   ├── RendererTypes.h
│       │   └── Backend/
│       │       └── DX11/
│       ├── Scene/
│       └── Texture/
├── LXShell/
│   └── Src/
│       └── ImGui/
└── Scripts/
    └── Audit-RendererBoundaries.ps1
```

**Structure Decision**: Use existing LongXi modular structure and introduce/maintain a backend-isolated renderer layer under `LXEngine/Src/Renderer/Backend/DX11`, with engine-facing renderer contracts at `LXEngine/Src/Renderer/Renderer.h` and `LXEngine/Src/Renderer/RendererTypes.h`.

## Phase 0: Research Focus

- Confirm best-practice contract for deterministic frame lifecycle and pass sequencing.
- Confirm explicit per-frame baseline state reset policy to eliminate leakage.
- Confirm backend encapsulation boundary patterns for handle/type exposure.
- Confirm safe resize and recovery behavior contract (queue-on-active-frame, apply-on-frame-start).

## Phase 1: Design Focus

- Define/validate entities and state machines in `data-model.md`.
- Define public renderer API and external pass bridge contracts in `contracts/`.
- Define validation and integration flow in `quickstart.md`.
- Update agent context for current feature vocabulary and constraints.

## Post-Design Constitution Re-Check

- **Article III baseline**: PASS (no baseline deviations introduced)
- **Article IV boundaries**: PASS (backend isolation and API narrowing explicitly enforced)
- **Article VII spec-first**: PASS (design artifacts aligned with spec and acceptance criteria)
- **Article IX runtime discipline**: PASS (single-thread-safe contract retained)
- **Article XI verification**: PASS (build/runtime/audit validation workflows defined)

**Gate Result (Post-Design)**: PASS

## Complexity Tracking

No constitutional violations requiring exception justification.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
