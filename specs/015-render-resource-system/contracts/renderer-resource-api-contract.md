# Contract: Renderer Resource API

## Purpose
Define engine-facing backend-agnostic API contracts for GPU resource creation, update, binding, and destruction.

## Public Include Surface
- `D:\Yamen Development\LongXi\LongXi\LXEngine\Src\Renderer\Renderer.h`
- `D:\Yamen Development\LongXi\LongXi\LXEngine\Src\Renderer\RendererTypes.h`

## Forbidden Public Leakage
Public API and engine-facing headers MUST NOT expose:
- `ID3D11Texture2D`
- `ID3D11Buffer`
- `ID3D11ShaderResourceView`
- Any type declared in `d3d11.h` / `dxgi.h`

## Resource Handles and Descriptors
- Handles are opaque identifiers only.
- Descriptors define all creation-time properties.
- Backend-native conversion of descriptor fields occurs inside renderer backend.

## API Operation Contract

| Operation | Preconditions | Guarantees |
|-----------|---------------|------------|
| `CreateTexture(TextureDesc)` | Renderer initialized, descriptor valid | Returns valid `TextureHandle` or explicit failure result |
| `DestroyTexture(TextureHandle)` | Handle resolves to live texture record | Invalidates handle generation and releases backend resource |
| `CreateVertexBuffer(BufferDesc)` | Descriptor type/usage valid | Returns valid `VertexBufferHandle` or failure result |
| `CreateIndexBuffer(BufferDesc)` | Descriptor type/usage valid | Returns valid `IndexBufferHandle` or failure result |
| `CreateConstantBuffer(BufferDesc)` | Descriptor size/alignment/usage valid | Returns valid `ConstantBufferHandle` or failure result |
| `DestroyBuffer(BufferHandle)` | Handle resolves to live buffer record | Buffer record transitions to destroyed state |
| `CreateVertexShader(ShaderDesc)` | Stage and bytecode source valid | Returns valid `ShaderHandle` for vertex stage |
| `CreatePixelShader(ShaderDesc)` | Stage and bytecode source valid | Returns valid `ShaderHandle` for pixel stage |
| `DestroyShader(ShaderHandle)` | Handle resolves to live shader record | Shader record destroyed and handle invalidated |
| `UpdateBuffer(UpdateRequest)` | Handle valid and usage policy permits update | Data upload applied or explicit failure returned |
| `MapBuffer(MapRequest)` | Handle valid, map mode compatible, not already mapped | Returns mapped access token/result |
| `UnmapBuffer(BufferHandle)` | Handle currently mapped | Buffer returns to non-mapped state |
| `BindVertexBuffer(...)` | Handle valid and type-compatible | IA stage binding updated deterministically |
| `BindIndexBuffer(...)` | Handle valid and type-compatible | IA stage binding updated deterministically |
| `BindConstantBuffer(...)` | Handle valid and type-compatible | Requested shader-stage constant slot updated |
| `BindShader(...)` | Handle valid and stage-compatible | Shader stage binding updated deterministically |
| `BindTexture(...)` | Handle valid and bind-compatible | Requested texture slot binding updated deterministically |

## Bind Stage Mapping Rules

- `BindVertexBuffer` targets IA vertex stream input only.
- `BindIndexBuffer` targets IA index stream only.
- `BindConstantBuffer` supports `VS` and `PS` stages with explicit slot index.
- `BindShader` resolves by shader-handle stage (`Vertex` -> VS, `Pixel` -> PS).
- `BindTexture` supports `VS` and `PS` shader-resource slots; invalid stage/resource combinations are rejected.
- Bind operations with invalid or stale handles MUST fail with `InvalidHandle` diagnostics.
- Bind operations with wrong resource kind/stage pairing MUST fail with `InvalidOperation` diagnostics.

## Failure Contract
- Invalid descriptor/handle operations are rejected.
- Rejections must produce diagnosable renderer logs.
- API operations must never silently fallback to backend-specific behavior.

## Boundary Compliance
- Engine systems call only this contract surface.
- Backend-specific object creation, conversion, and binding calls remain in DX11 backend implementation modules.

## Reference Implementation Rule
- Legacy reference inspection under `D:\Yamen Development\Old-Reference\cqClient\Conquer` is behavior-guidance only.
- Contract authority is Spec 015 + LongXi architecture.
