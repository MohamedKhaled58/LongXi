# Data Model: Spec 015 - Render Resource System

## Entity: ResourceHandle

### Purpose
Backend-agnostic opaque identifier used by engine systems to reference renderer-owned GPU resources.

### Fields
- `Kind`: `Texture | VertexBuffer | IndexBuffer | ConstantBuffer | VertexShader | PixelShader`
- `Slot`: integer pool index
- `Generation`: integer version for stale-handle detection

### Validation Rules
- Handle is valid only when `Kind` matches operation type and `(Slot, Generation)` resolves to live record.
- Zero/invalid slot values are rejected.
- Handle generation mismatch is treated as stale-handle access.

## Entity: TextureDesc

### Purpose
Descriptor for texture creation.

### Fields
- `Width`: integer
- `Height`: integer
- `Format`: engine-defined texture format
- `Usage`: `Static | Dynamic | Staging`
- `CpuAccess`: `None | Read | Write | ReadWrite`
- `BindFlags`: set containing `ShaderResource | RenderTarget | DepthStencil`
- `InitialData`: optional byte payload reference

### Validation Rules
- Width/Height must be > 0.
- Format must be supported by backend mapping.
- `CpuAccess` must be compatible with `Usage`.
- Bind flags must not be empty for non-staging GPU resources.

## Entity: BufferDesc

### Purpose
Descriptor for vertex/index/constant buffer creation.

### Fields
- `BufferType`: `Vertex | Index | Constant`
- `ByteSize`: integer
- `Stride`: integer (required for vertex/index)
- `Usage`: `Static | Dynamic | Staging`
- `CpuAccess`: `None | Read | Write | ReadWrite`
- `BindFlags`: set containing buffer-compatible bind intents
- `InitialData`: optional byte payload reference

### Validation Rules
- Byte size must be > 0.
- Constant buffers must satisfy backend alignment requirements.
- `Stride` must be valid for typed buffers.
- `CpuAccess`/`Usage` compatibility enforced.

## Entity: ShaderDesc

### Purpose
Descriptor for shader resource creation.

### Fields
- `Stage`: `Vertex | Pixel`
- `BytecodeSource`: `MemoryBlob | FilePath`
- `EntryPoint`: optional symbol name when source requires compile step
- `Profile`: shader profile identifier

### Validation Rules
- Stage must match create operation.
- Bytecode source must resolve to non-empty bytecode.
- Descriptor combination must be backend-compilable/loadable.

## Entity: ResourceRecord

### Purpose
Internal renderer table entry for a live GPU resource.

### Fields
- `Handle`: `ResourceHandle`
- `State`: `Allocated | Bound | Mapped | PendingDestroy | Destroyed`
- `Usage`: resource usage class
- `DebugName`: optional diagnostics label
- `CreatedFrameIndex`: integer
- `LastBoundFrameIndex`: integer

### Validation Rules
- State transitions must be legal for operation class.
- Mapped state is exclusive for each record.
- Destroyed records are inaccessible except for diagnostics.

### State Transitions
- `Allocated -> Bound` on valid bind
- `Allocated -> Mapped` on valid map
- `Mapped -> Allocated` on unmap
- `Bound -> Allocated` when unbound/replaced
- `Allocated|Bound -> PendingDestroy -> Destroyed` on destroy workflow

## Entity: ResourcePool

### Purpose
Renderer-owned collection storing records and backend-native objects for one resource class.

### Fields
- `PoolKind`: `TexturePool | BufferPool | ShaderPool`
- `Records`: indexed collection of `ResourceRecord`
- `FreeList`: reusable slot indices
- `LiveCount`: integer

### Validation Rules
- Slot reuse must increment generation.
- Live count must match non-destroyed record count.
- Destroyed records cannot remain in active lookup path.

## Entity: BindingRequest

### Purpose
Normalized bind operation request consumed by renderer binding APIs.

### Fields
- `Handle`: `ResourceHandle`
- `PipelineStage`: `IA | VS | PS | OM`
- `BindPoint`: stage-specific slot index

### Validation Rules
- Resource kind must be compatible with target stage.
- Bind point must be within backend limits.
- Invalid handles or mismatched kind/stage combinations are rejected.

## Entity: UpdateRequest

### Purpose
Structured CPU↔GPU update request for dynamic/staging resources.

### Fields
- `Handle`: `ResourceHandle`
- `UpdateMode`: `UpdateSubresource | MapWriteDiscard | MapWriteNoOverwrite | MapRead`
- `ByteOffset`: integer
- `ByteLength`: integer
- `SourceData`: byte payload reference

### Validation Rules
- Update mode must be compatible with resource usage policy.
- Offset + length must remain in resource bounds.
- Map requests on already-mapped records are rejected.

## Relationships
- `ResourceHandle` resolves into `ResourceRecord` entries held by `ResourcePool`.
- `TextureDesc`, `BufferDesc`, and `ShaderDesc` produce typed `ResourceHandle` instances on successful create operations.
- `BindingRequest` and `UpdateRequest` operate only on valid live `ResourceRecord` entries.
- `ResourceRecord.State` drives whether bind/update/destroy operations are legal.
- Renderer shutdown walks every `ResourcePool` and transitions remaining live records to `Destroyed`.

## Reference Implementation Rule
- Reference implementations under `D:\Yamen Development\Old-Reference\cqClient\Conquer` may inform behavior constraints only.
- Data model authority remains LongXi architecture and Spec 015 requirements.
