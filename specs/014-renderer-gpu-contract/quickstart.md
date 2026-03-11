# Quickstart: Spec 014 - Renderer API and Backend Architecture

## Goal
Validate backend-agnostic renderer API behavior, deterministic GPU state ownership, and boundary compliance.

## Prerequisites
- Windows development environment with Visual Studio 2026 toolchain.
- Project generated from repository root.
- Development logging enabled.

## Build and Run
1. Build project:
   - `Win-Build Project.bat`
2. Launch runtime:
   - `Build\Debug\Executables\LXShell.exe`
3. Run boundary audit:
   - `powershell -ExecutionPolicy Bypass -File Scripts/Audit-RendererBoundaries.ps1 -Json`

## Validation Scenarios

### Scenario 1: Deterministic frame lifecycle
- Run Scene + Sprite + DebugUI paths simultaneously.
- Keep runtime active for long-run validation.
- Verify no lifecycle-order contract violations.

### Scenario 2: GPU state ownership and reset
- Exercise scene and sprite rendering across repeated frames.
- Verify engine rendering remains stable regardless of external pass activity.
- Verify no subsystem relies on implicit previous-frame state.

### Scenario 3: Resize robustness
- Execute repeated resize/minimize/restore operations.
- Include active-frame resize events.
- Verify renderer recovers viewport/render-target correctness after valid size restoration.

### Scenario 4: Boundary compliance
- Run `Audit-RendererBoundaries.ps1`.
- Verify no DirectX include/type leakage outside renderer backend implementation modules.

### Scenario 5: Failure-path handling
- Trigger or simulate present/device/swapchain failure path where feasible.
- Verify logging + safe-mode transition + controlled recovery behavior.

## Expected Outcome
- Stable rendering without blank-frame regressions.
- No lifecycle-order violations during normal operation.
- Reliable resize handling and recovery.
- No backend leakage in engine system boundaries.
- Diagnosable logs for lifecycle, resize, and recovery events.

## Validation Evidence (2026-03-11)
- Boundary audit: PASS (`{"scanned":"LongXi/LXEngine/Src","violations":[],"ok":true}`).
- Runtime evidence: validation scene and Debug UI panel logs were observed in local runs provided during implementation (`[ValidationScene] Test sprite node attached`, `[DebugUI] Engine panel opened`, `[DebugUI] Scene inspector opened`).
- Resize evidence: runtime logs indicated resize callbacks and stable renderer viewport updates after active-window resize operations.
- Build caveat in this automation environment: `Win-Build Project.bat` failed with `MSB4018` (MSBuild FileTracker `IndexOutOfRangeException`) before project compilation completed; this appears environment/tooling-related rather than a code compile diagnostic.

## Recommended Local Verification (Developer Machine)
1. Run `Win-Build Project.bat` from repository root.
2. Run `Build\Debug\Executables\LXShell.exe` and exercise Scene + Sprite + DebugUI for long-run rendering.
3. Perform repeated resize/minimize/restore operations and verify recovery behavior.
4. Run `powershell -ExecutionPolicy Bypass -File Scripts/Audit-RendererBoundaries.ps1 -Json` and confirm `ok=true`.
