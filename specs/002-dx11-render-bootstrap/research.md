# Research: DX11 Render Bootstrap

**Date**: 2026-03-10
**Spec**: [spec.md](spec.md)

## R1: DX11 Device and Swap Chain Creation Pattern

**Decision**: Use `D3D11CreateDeviceAndSwapChain` as the single-call initialization path.

**Rationale**: This is the standard DX11 bootstrap pattern for single-window applications. It creates the device, device context, and swap chain in one call. Separating device creation from swap chain creation (via `IDXGIFactory::CreateSwapChain`) is only needed when multiple swap chains or advanced DXGI control is required — not applicable to this bootstrap.

**Alternatives considered**:

- Separate `D3D11CreateDevice` + `IDXGIFactory::CreateSwapChain`: More flexible but unnecessary complexity for bootstrap. Can migrate later if needed.
- `D3D11CreateDeviceAndSwapChain` with feature level array: Selected. Pass `D3D_FEATURE_LEVEL_11_0` only since we mandate DX11.

**Key parameters**:

- `DriverType`: `D3D_DRIVER_TYPE_HARDWARE` (GPU rendering)
- `Flags`: `D3D11_CREATE_DEVICE_DEBUG` in Debug builds only (OQ-001 resolved)
- `FeatureLevels`: `{ D3D_FEATURE_LEVEL_11_0 }` — single level, no fallback
- `SDKVersion`: `D3D11_SDK_VERSION`

## R2: Swap Chain Configuration

**Decision**: Use `DXGI_SWAP_CHAIN_DESC` with double buffering and `DXGI_FORMAT_R8G8B8A8_UNORM`.

**Rationale**: Standard swap chain setup for DX11 desktop applications. Double buffering (1 back buffer) with flip-discard is the modern default. `R8G8B8A8_UNORM` is universally supported.

**Swap chain parameters**:

- `BufferCount`: 1 (single back buffer for double buffering)
- `BufferDesc.Format`: `DXGI_FORMAT_R8G8B8A8_UNORM`
- `BufferDesc.Width/Height`: Window client area dimensions
- `BufferUsage`: `DXGI_USAGE_RENDER_TARGET_OUTPUT`
- `OutputWindow`: Existing HWND from Win32Window
- `SampleDesc`: `{ Count = 1, Quality = 0 }` (no MSAA for bootstrap)
- `Windowed`: `TRUE`
- `SwapEffect`: `DXGI_SWAP_EFFECT_DISCARD` (simplest for single back buffer)

**Alternatives considered**:

- `DXGI_SWAP_EFFECT_FLIP_DISCARD`: Modern recommended path but requires `BufferCount >= 2` and DXGI 1.4. Can migrate in a later spec.
- Triple buffering: Unnecessary for bootstrap clear/present.

## R3: Render Target View Creation

**Decision**: Acquire back buffer via `IDXGISwapChain::GetBuffer(0)`, create `ID3D11RenderTargetView` from it.

**Rationale**: Standard pattern. The back buffer texture is obtained from the swap chain, then a render target view is created to bind it for clearing.

**Pattern**:

```cpp
ComPtr<ID3D11Texture2D> backBuffer;
swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
```

## R4: Per-Frame Clear and Present

**Decision**: `ClearRenderTargetView` with cornflower blue (`0.392f, 0.584f, 0.929f, 1.0f`) followed by `Present(1, 0)`.

**Rationale**: Cornflower blue is the traditional DX bootstrap clear color (easily distinguishable from black brush background). `Present(1, 0)` = VSync enabled (OQ-002 resolved).

**Frame sequence**:

1. `OMSetRenderTargets(1, &rtv, nullptr)` — bind render target (no depth buffer per OQ-003)
2. `ClearRenderTargetView(rtv, clearColor)` — clear to cornflower blue
3. `Present(1, 0)` — present with VSync

## R5: Resize Handling

**Decision**: On `WM_SIZE`, release render target view and back buffer reference, call `ResizeBuffers`, re-acquire back buffer and recreate render target view.

**Rationale**: Standard DX11 resize pattern. All references to the back buffer must be released before `ResizeBuffers` can succeed. Minimized windows (zero-area) should skip resize to avoid `DXGI_ERROR_INVALID_CALL`.

**Resize sequence**:

1. Check if new size is zero (minimized) — if so, skip and set a flag
2. Release render target view (`ReleaseRenderTarget()`)
3. Call `swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)`
4. Re-acquire back buffer and recreate render target view (`CreateRenderTarget()`)

## R6: COM Object Ownership

**Decision**: Use `Microsoft::WRL::ComPtr<T>` (from `<wrl/client.h>`) for all DX11/DXGI COM objects.

**Rationale**: ComPtr provides RAII semantics for COM objects — automatic `Release()` in destructor, no leaked references, exception-safe. This is the standard modern C++ pattern for DX11 (used by Microsoft's own samples). No raw `AddRef/Release` needed.

**Objects managed**:

- `ComPtr<ID3D11Device>` m_Device
- `ComPtr<ID3D11DeviceContext>` m_Context
- `ComPtr<IDXGISwapChain>` m_SwapChain
- `ComPtr<ID3D11RenderTargetView>` m_RenderTargetView

## R7: Debug Layer

**Decision**: Enable `D3D11_CREATE_DEVICE_DEBUG` flag when `LX_DEBUG` is defined (Debug configuration only).

**Rationale**: The DX11 debug layer reports resource leaks, invalid API usage, and parameter errors. Essential for development. Disabled in Release/Dist to avoid performance overhead.

**Pattern**:

```cpp
UINT createFlags = 0;
#ifdef LX_DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
```

## R8: Build System Integration

**Decision**: Add `d3d11` and `dxgi` to LXEngine's premake5.lua `links` block. Also add `dxguid` for debug interfaces.

**Rationale**: LXEngine owns the renderer. These are Windows SDK system libraries — no third-party vendor dependency.

**Links required**:

- `d3d11.lib` — DX11 device and context
- `dxgi.lib` — DXGI swap chain and factory
- `dxguid.lib` — COM GUIDs for debug interfaces

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
