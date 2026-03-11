# Contract: Renderer Public API (Backend-Agnostic)

## Purpose
Define the engine-facing renderer contract while keeping backend implementation details encapsulated.

## Public Include Surface
- `LongXi/LXEngine/Src/Renderer/Renderer.h`
- `LongXi/LXEngine/Src/Renderer/RendererTypes.h`

## Public API Surface
- `Renderer` is an abstract backend-agnostic contract.
- `CreateRenderer()` is the renderer-factory entrypoint used by engine runtime code.
- `DX11Renderer` remains an internal backend implementation behind `Renderer`.

## Forbidden Public Leakage
Public engine headers and renderer public contracts MUST NOT expose:
- `ID3D11Device`
- `ID3D11DeviceContext`
- `D3D11_VIEWPORT`
- `DXGI_SWAP_CHAIN_DESC`
- `DXGI_FORMAT`

Engine modules outside renderer backend MUST NOT include:
- `d3d11.h`
- `dxgi.h`

## Operations and Guarantees

| Operation | Preconditions | Guarantees |
|-----------|---------------|------------|
| `Initialize(hwnd, width, height)` | Renderer not initialized; valid window and dimensions | Backend surface/device resources ready; lifecycle reset |
| `BeginFrame()` | Lifecycle in frame-start-allowed state | Baseline GPU state explicitly rebound; pending resize applied if valid |
| `Clear(color)` | Active frame | Active render target cleared according to API input |
| `SetViewport(viewport)` | Active frame/pass according to policy | Renderer viewport state updated through backend |
| `SetRenderTarget()` | Active frame/pass according to policy | Backbuffer/depth binding updated through backend |
| `DrawIndexed(count, ...)` | Active pass and valid bindings | Indexed draw submitted through backend |
| `EndFrame()` | No active pass | Frame lifecycle transitions to end state |
| `Present()` | Frame ended | Swapchain present attempted under recovery policy |
| `OnResize(width, height)` | Renderer initialized | Resize queued or applied safely; viewport/resources updated |

## Frame Lifecycle Contract
Required order:
1. `BeginFrame`
2. Frame/pass rendering operations
3. `EndFrame`
4. `Present`

Invalid order must be rejected and logged.

## GPU State Ownership Contract
After `BeginFrame`, renderer guarantees baseline validity for:
- Render target
- Depth stencil
- Viewport
- Rasterizer state
- Blend state
- Depth state

No caller may rely on state persistence from prior frame/pass.

## Resize Contract
- Mid-frame resize is queued.
- Next safe frame boundary applies resize.
- Zero-size states do not trigger invalid resource recreation.

## Recovery Contract
On present/device-loss/swapchain failure:
- Log error context.
- Enter safe non-rendering mode.
- Attempt controlled reinitialization based on recovery triggers.

## Boundary Compliance Contract
- Engine systems consume renderer API only.
- Backend-specific API use is confined to renderer backend implementation modules.
- Boundary audit script validates compliance.

## Implemented Handle Contract
- `RendererTextureHandle`, `RendererBufferHandle`, and `RendererShaderHandle` use opaque ownership (`std::shared_ptr<void>`).
- Public renderer handles expose no DirectX-native symbols or headers.
