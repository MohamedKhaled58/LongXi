# Implementation Plan: Spec 014 - Renderer API and GPU State Contract

**Branch**: `014-renderer-gpu-contract` | **Date**: 2026-03-11 | **Spec**: `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\spec.md`
**Input**: Feature specification from `D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\spec.md`

## Summary

Define and implement a backend-agnostic renderer API contract that makes the renderer the sole owner of GPU state, enforces deterministic frame lifecycle and pass sequencing, and prevents DirectX leakage outside renderer backend implementation. The plan covers lifecycle validation, state reset guarantees, queued resize processing, external-pass bridging for Debug UI, and recoverable device/present failure behavior.

## Technical Context

**Language/Version**: C++23 (MSVC v145)  
**Primary Dependencies**: Win32 API, DirectX 11 runtime (renderer backend internal only), Spdlog logging, Dear ImGui (LXShell-side consumer via renderer bridge)  
**Storage**: N/A (in-memory runtime state only)  
**Testing**: Existing runtime validation harness + frame/resize/failure smoke scenarios + architecture compliance audit (header/include boundary checks)  
**Target Platform**: Windows desktop (Win32), Visual Studio 2026 toolchain  
**Project Type**: Native desktop engine + shell application  
**Performance Goals**: Stable frame lifecycle at runtime; no renderer-state-related blank frames in 10,000-frame validation run; resize recovery within next valid frame cycle  
**Constraints**: DX11 only in Phase 1; single-threaded runtime in Phase 1; no DirectX headers/types outside renderer backend; dependency direction `LXShell -> LXEngine -> LXCore`  
**Scale/Scope**: Single active render surface, three defined passes (Scene/Sprite/Debug UI), 100-iteration resize stress coverage, contract violation handling for all in-scope renderer API entry points

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Research Gate Review

- **Article III (Technical Baseline)**: PASS  
  DirectX 11, Win32, MSVC v145, C++23, and static-library architecture remain unchanged.
- **Article IV (Architectural Laws)**: PASS  
  Ownership boundary strengthened: renderer owns GPU state; engine modules consume narrow renderer API.
- **Article VII (Specification-First)**: PASS  
  Work directly traces to approved spec requirements FR-001..FR-019.
- **Article IX (Threading Discipline)**: PASS  
  Design remains single-threaded for Phase 1 and future MT-aware.
- **Article XI (Verification and Honesty)**: PASS  
  Plan includes explicit validation scenarios and failure-path diagnostics.
- **Article XII (Phase 1 Guardrails)**: PASS  
  Scope stays within native client foundation and renderer/runtime correctness.

**Gate Result (Pre-Research)**: PASS

### Post-Design Gate Review

- **Design artifacts preserve module boundaries**: PASS
- **No constitutional violations introduced by contracts/data model**: PASS
- **Phase scope remains constrained to renderer contract/system integration**: PASS

**Gate Result (Post-Design)**: PASS

## Project Structure

### Documentation (this feature)

```text
D:\Yamen Development\LongXi\specs\014-renderer-gpu-contract\
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts\
│   ├── renderer-api-contract.md
│   └── external-pass-bridge-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
D:\Yamen Development\LongXi\LongXi\
├── LXCore\Src\
│   └── Core\Logging\
├── LXEngine\Src\
│   ├── Engine\
│   ├── Renderer\
│   ├── Scene\
│   └── Texture\
└── LXShell\Src\
    ├── DebugUI\
    └── ImGui\
```

**Structure Decision**: Keep existing module layout. Introduce/modify renderer contract surfaces in `LXEngine\Src\Renderer\` and integrate consumers in `LXEngine\Src\Engine\`, `LXEngine\Src\Scene\`, `LXEngine\Src\Texture\`, and `LXShell\Src\DebugUI\` without changing dependency direction or exposing backend-native types.

## Phase 0: Research Plan

- Research deterministic GPU-state baseline reset strategy per frame/pass for DX11 backend internals.
- Research boundary pattern for backend-agnostic external pass bridge (Debug UI integration without backend leakage).
- Research resilient resize and present/device-loss recovery flow compatible with current engine loop.
- Research contract violation handling policy for debug vs non-debug behavior.

Outputs captured in `research.md` with explicit decisions, rationale, and alternatives.

## Phase 1: Design Plan

- Derive renderer contract data model and lifecycle state model in `data-model.md`.
- Define interface contracts in `contracts/` for:
  - Renderer public API lifecycle and pass operations.
  - External pass bridge operations and constraints.
- Produce `quickstart.md` with validation scenarios for frame lifecycle, resize, boundary checks, and failure recovery.
- Update agent context using project script.

## Phase 2: Task Planning Approach

- Generate executable tasks grouped by user story priority (P1 deterministic lifecycle, P2 resize robustness, P3 boundary enforcement).
- Enforce dependency ordering: foundation first (API + state contract), then integration, then validation/polish.

## Complexity Tracking

No constitution violations or exception requests identified; this section remains intentionally empty.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
