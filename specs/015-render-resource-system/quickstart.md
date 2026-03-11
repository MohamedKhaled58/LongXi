# Quickstart: Spec 015 - Render Resource System

## Goal
Validate descriptor-based renderer resource APIs, opaque-handle boundaries, update/bind safety, and deterministic lifetime cleanup.

## Prerequisites
- Windows development environment with Visual Studio 2026 + MSVC v145.
- Project generated at `D:\Yamen Development\LongXi`.
- Development logging enabled.

## Build and Validation Commands
1. Build solution:
   - `D:\Yamen Development\LongXi\Win-Build Project.bat`
2. Run shell runtime:
   - `D:\Yamen Development\LongXi\Build\Debug\Executables\LXShell.exe`
3. Run boundary audit:
   - `powershell -ExecutionPolicy Bypass -File D:\Yamen Development\LongXi\Scripts\Audit-RendererBoundaries.ps1 -Json`
4. Run descriptor-creation validation:
   - `powershell -ExecutionPolicy Bypass -File D:\Yamen Development\LongXi\Scripts\Validate-Spec015-ResourceCreation.ps1 -Json`
5. Run update/binding validation:
   - `powershell -ExecutionPolicy Bypass -File D:\Yamen Development\LongXi\Scripts\Validate-Spec015-UpdatesAndBinding.ps1 -Json`
6. Run lifetime/shutdown validation:
   - `powershell -ExecutionPolicy Bypass -File D:\Yamen Development\LongXi\Scripts\Validate-Spec015-Lifetime.ps1 -Json`

## Validation Scenarios

### Scenario 1: Descriptor-based resource creation
- Create texture, vertex buffer, index buffer, constant buffer, vertex shader, and pixel shader through renderer descriptor APIs.
- Verify each operation returns valid opaque handles.
- Verify no engine module requires DirectX includes/types.
- Verify `Validate-Spec015-ResourceCreation.ps1` returns `ok: true`.

### Scenario 2: Descriptor rejection behavior
- Submit invalid descriptors (zero size, unsupported format, incompatible usage/access flags).
- Verify renderer rejects each request and logs reason context.

### Scenario 3: Dynamic update policy
- Create dynamic buffer and perform update/map/unmap operations.
- Attempt disallowed update mode for static resource.
- Verify valid operations succeed and invalid ones are rejected with logs.
- Verify `Validate-Spec015-UpdatesAndBinding.ps1` reports update-policy checks as pass.

### Scenario 4: Binding contract
- Bind vertex/index/constant buffers, shaders, and textures in expected pass flow.
- Verify stage/bind-point compatibility checks are enforced.
- Verify invalid kind/stage combinations are rejected.
- Verify binding checks in `Validate-Spec015-UpdatesAndBinding.ps1` return pass.

### Scenario 5: Lifetime and stale-handle safety
- Create and destroy resources repeatedly (>= 1,000 operations per class).
- Reuse destroyed handles intentionally.
- Verify stale/unknown handles are rejected and do not crash runtime.
- Verify `Validate-Spec015-Lifetime.ps1` reports generation and stale-handle checks as pass.

### Scenario 6: Shutdown cleanup
- Leave resources live at runtime shutdown.
- Verify renderer shutdown force-releases remaining resources and logs cleanup summary.
- Verify cleanup summary reports `live=0` for texture, buffer, and shader pools.

## Expected Outcomes
- 100% resource creation through descriptor APIs and opaque handles.
- Zero DirectX leakage in engine-facing modules.
- Deterministic rejection for invalid descriptor/handle/update/bind operations.
- No stale-handle crash in stress scenario.
- Renderer shutdown completes with zero unreleased live resources.

## Reference Implementation Rule
- Any reference inspection under `D:\Yamen Development\Old-Reference\cqClient\Conquer` is for behavior constraints only.
- Final design/validation remains LongXi architecture-driven.
