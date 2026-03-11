# Quickstart: Spec 014 - Renderer API and GPU State Contract

## Goal
Validate renderer lifecycle ordering, GPU state determinism, resize robustness, and renderer-boundary compliance.

## Prerequisites
- Windows with VS2026 toolchain.
- Solution generated from repository root.
- Development logging enabled.

## Commands
1. Build:
   - `Win-Build Project.bat`
2. Run boundary audit:
   - `powershell -ExecutionPolicy Bypass -File Scripts/Audit-RendererBoundaries.ps1 -Json`
3. Run executable:
   - `Build\Debug\Executables\LXShell.exe`

## Validation Scenarios and Results

### Scenario 1: Deterministic frame lifecycle (P1)
Target: Scene + Sprite + Debug UI with stable rendering and no lifecycle-order errors.

Result: PASS (runtime logs show renderer, scene, sprite, camera, and Debug UI startup/usage without contract-violation errors).

### Scenario 2: External pass boundary integrity (P1/P3)
Target: Debug UI rendered via renderer-owned external pass while engine rendering remains stable.

Result: PASS (Debug UI panels open and rendering path remains stable in runtime logs).

### Scenario 3: Resize stress and recovery (P2)
Target: Repeated resize/minimize/restore with correct surface/viewport behavior.

Result: PASS (manual resize testing confirmed stable behavior and correct recovery; no rendering breakage observed).

### Scenario 4: Boundary audit (P3)
Target: No backend leakage outside renderer module.

Result: PASS (`Scripts/Audit-RendererBoundaries.ps1 -Json` returned `ok=true`, no violations).

### Scenario 5: Build and integration verification
Target: Full debug build success with current implementation.

Result: PASS (Debug x64 build completed: `LXCore`, `LXEngine`, `LXShell` all succeeded).

## Observed Runtime Evidence (2026-03-11)
- `[Renderer] Device initialized`
- `[SpriteRenderer] Initialized`
- `[Scene] Initialized`
- `[Camera] View matrix updated`
- `[Camera] Projection updated`
- `[ImGuiLayer] Initialized`
- `[DebugUI] Engine/Scene/Texture/Input panels opened`
- Validation scene texture load success (`data/texture/1.dds`, 256x256, DXT3)

## Pass Criteria
- No lifecycle contract violations during normal rendering.
- No backend API leakage outside renderer module.
- No blank-frame regressions after external/debug passes.
- Resize path recovers rendering deterministically.
