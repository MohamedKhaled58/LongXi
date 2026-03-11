# Feature Specification: Sprite Rendering System

**Feature Branch**: `010-sprite-rendering-system`
**Created**: 2026-03-11
**Status**: Draft
**Input**: User description: "Spec 010 — Sprite Rendering System: Introduce a SpriteRenderer subsystem capable of rendering textured quads using the existing DX11 renderer. Integrates with TextureManager, screen-space coordinates, texture batching, Begin/DrawSprite/End lifecycle, GPU vertex and index buffers, and resize handling."

## Clarifications

### Session 2026-03-11

- Q: Should sprites render before or after the future 3D scene in Engine::Render()? → A: After the scene — sprites render last, always on top (FR-030 updated)
- Q: Where should Vector2 and Color types be defined? → A: New shared `LongXi/LXEngine/Src/Math/Math.h` header within LXEngine (FR-017, FR-018, FR-038, Assumption 3 updated)
- Q: If shader compilation fails during SpriteRenderer::Initialize(), should it be fatal or graceful? → A: Graceful — log error, mark disabled, engine continues without sprites (FR-005, FR-031a, FR-034 updated)

## User Scenarios & Testing

### User Story 1 — Render a Textured Sprite on Screen (Priority: P1)

As an engine developer, I want to draw a textured quad at a specific screen-space position and size so that I can display images on screen using the engine.

**Why this priority**: This is the fundamental deliverable of the entire spec. Without the ability to draw a single textured quad, nothing else in the sprite system has value. All other user stories depend on this working correctly.

**Independent Test**: Can be fully tested by loading a texture from TextureManager, calling `SpriteRenderer::Begin()`, `DrawSprite(texture, position, size)`, and `End()`, and verifying that the textured quad appears at the correct screen position with correct dimensions.

**Acceptance Scenarios**:

1. **Given** the Engine is initialized and a texture is loaded via TextureManager, **When** `DrawSprite(texture, {100, 200}, {64, 64})` is called between `Begin()` and `End()`, **Then** a 64×64 pixel quad textured with the loaded image appears at screen position (100, 200)
2. **Given** a sprite is drawn at position (0, 0), **When** the frame is rendered, **Then** the sprite appears in the top-left corner of the window
3. **Given** a sprite is drawn at position (windowWidth - size.x, windowHeight - size.y), **When** the frame is rendered, **Then** the sprite appears in the bottom-right corner of the window
4. **Given** `DrawSprite` is called outside of a `Begin()`/`End()` pair, **When** the call is made, **Then** the call is ignored (or an error is logged) and no draw occurs

---

### User Story 2 — Batch Multiple Sprites in a Single Draw Call (Priority: P1)

As an engine developer, I want the SpriteRenderer to automatically batch sprites that share the same texture into a single GPU draw call so that rendering performance scales with sprite count rather than degrading linearly.

**Why this priority**: Batching is the core performance mechanism for 2D rendering. Without batching, each sprite requires a separate GPU draw call, making scenes with hundreds of sprites impractical. This is required for the system to be viable as a foundation for UI and gameplay rendering.

**Independent Test**: Can be tested by drawing 32 sprites with the same texture between `Begin()` and `End()`, and verifying that the log reports "Batch flushed (32 sprites)" — indicating a single batched draw call rather than 32 individual calls.

**Acceptance Scenarios**:

1. **Given** 32 sprites all sharing the same texture are submitted in one `Begin()`/`End()` block, **When** `End()` is called, **Then** exactly one GPU draw call is issued and the log reports `[SpriteRenderer] Batch flushed (32 sprites)`
2. **Given** 512 sprites sharing the same texture are submitted (exceeding the batch limit of 1024 sprites), **When** the batch limit is reached during DrawSprite calls, **Then** the current batch is automatically flushed and a new batch begins transparently to the caller
3. **Given** sprites alternating between two different textures are submitted, **When** `End()` flushes, **Then** a new batch begins each time the texture changes, resulting in one draw call per texture group

---

### User Story 3 — Render Sprites with Custom UV Coordinates (Priority: P2)

As an engine developer, I want to draw a sub-region of a texture (UV-mapped quad) so that I can use texture atlases and sprite sheets efficiently.

**Why this priority**: UV control is essential for texture atlases, which are the standard approach for game sprites. Without UV support, every sprite requires its own full texture, which is impractical for real game assets.

**Independent Test**: Can be tested by drawing a sprite with `UVMin = {0.0, 0.0}` and `UVMax = {0.5, 0.5}` from a checkerboard texture, and verifying that only the top-left quadrant of the texture appears on screen.

**Acceptance Scenarios**:

1. **Given** a texture is 256×256, **When** a sprite is drawn with `UVMin = {0.0, 0.0}` and `UVMax = {0.5, 0.5}`, **Then** only the top-left 128×128 region of the texture is rendered on the quad
2. **Given** a default DrawSprite call without explicit UV arguments, **When** rendered, **Then** the full texture is mapped to the quad (`UVMin = {0.0, 0.0}`, `UVMax = {1.0, 1.0}`)

---

### User Story 4 — Render Sprites with Per-Sprite Color Tinting (Priority: P2)

As an engine developer, I want to tint a sprite with a color (including alpha for transparency) so that I can fade, colorize, or partially-transparent sprites without separate textures.

**Why this priority**: Per-sprite color tinting enables visual effects (fading, highlighting, damage flash) without creating duplicate textures. Alpha support is required for UI elements and overlays.

**Independent Test**: Can be tested by drawing a white sprite with color `{1.0, 0.0, 0.0, 1.0}` (opaque red tint) and verifying the sprite appears red on screen, and with `{1.0, 1.0, 1.0, 0.5}` (50% opacity) to verify semi-transparency.

**Acceptance Scenarios**:

1. **Given** a white texture sprite is drawn with color `{1.0, 0.0, 0.0, 1.0}`, **When** rendered, **Then** the sprite appears red
2. **Given** a sprite is drawn with alpha `0.5`, **When** rendered, **Then** the sprite appears at 50% opacity, blending with the background
3. **Given** a sprite is drawn with the default color `{1.0, 1.0, 1.0, 1.0}` (white opaque), **When** rendered, **Then** the texture appears without any color modification

---

### User Story 5 — Projection Matrix Updates on Window Resize (Priority: P2)

As an engine developer, I want sprite screen-space coordinates to remain correct after the window is resized so that UI and sprites stay positioned correctly regardless of window dimensions.

**Why this priority**: Without resize handling, all sprite positions and sizes become incorrect when the window is resized, making the system unusable for any production scenario. This connects to the established resize event routing from Spec 009.

**Independent Test**: Can be tested by drawing a sprite at position (0, 0) with size (100, 100), resizing the window, drawing the same sprite again, and verifying the sprite occupies the same proportional screen area (top-left 100×100 pixels of the new window size).

**Acceptance Scenarios**:

1. **Given** a sprite is drawn at position (0, 0) in a 1024×768 window, **When** the window is resized to 1920×1080, **Then** the sprite still appears at the top-left pixel of the window
2. **Given** the window resize event fires, **When** `SpriteRenderer::OnResize(newWidth, newHeight)` is called, **Then** the orthographic projection matrix is updated to match the new dimensions
3. **Given** `OnResize` is called with zero or negative dimensions, **When** the call is processed, **Then** the resize is ignored and the previous projection matrix is retained

---

### Edge Cases

- What happens when `DrawSprite` is called with a null texture pointer?
- What happens when `DrawSprite` is called with zero or negative width/height?
- What happens when `DrawSprite` is called outside of a `Begin()`/`End()` pair?
- What happens when `End()` is called without a preceding `Begin()`?
- What happens when `Begin()` is called twice without `End()`?
- What happens when the batch limit (1024 sprites) is exceeded in a single frame?
- What happens when `SpriteRenderer::Begin()` is called during Engine shutdown?
- What happens when a texture is destroyed while it is still referenced by a submitted sprite?
- What happens when `OnResize` is called with zero-area dimensions (minimized window)?
- What happens when no sprites are submitted between `Begin()` and `End()`?

---

## Requirements

### Functional Requirements

- **FR-001**: The system MUST introduce a `SpriteRenderer` class as an Engine subsystem, owned and initialized by the Engine after TextureManager
- **FR-002**: The system MUST expose `Engine::GetSpriteRenderer()` accessor that returns a valid `SpriteRenderer&` when the Engine is initialized
- **FR-003**: The system MUST initialize `SpriteRenderer` in this order: (5) SpriteRenderer — after Renderer, InputSystem, VirtualFileSystem, and TextureManager
- **FR-004**: The system MUST update `Engine::Shutdown()` to destroy `SpriteRenderer` first (before TextureManager) in reverse initialization order
- **FR-005**: The system MUST provide `SpriteRenderer::Initialize(DX11Renderer& renderer, int width, int height)` that creates all GPU pipeline state (shaders, vertex buffer, index buffer, constant buffer, blend state, depth state, sampler state, rasterizer state). If shader compilation or any GPU resource creation fails, `Initialize()` MUST log an error, mark the SpriteRenderer as disabled, and return false — but this failure MUST NOT propagate to `Engine::Initialize()` (the engine continues to start without sprite support)
- **FR-006**: The system MUST provide `SpriteRenderer::Shutdown()` that releases all GPU resources
- **FR-007**: The system MUST provide `SpriteRenderer::Begin()` that marks the start of a sprite submission block for the current frame
- **FR-008**: The system MUST provide `SpriteRenderer::End()` that flushes all pending sprites to the GPU and resets the batch buffer
- **FR-009**: The system MUST provide `SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size)` with a default full-texture UV and opaque white color
- **FR-010**: The system MUST provide `SpriteRenderer::DrawSprite(Texture* texture, Vector2 position, Vector2 size, Vector2 uvMin, Vector2 uvMax, Color color)` that draws a UV-mapped, color-tinted sprite
- **FR-011**: The system MUST provide `SpriteRenderer::OnResize(int width, int height)` that updates the orthographic projection matrix to match the new window dimensions
- **FR-012**: The system MUST call `SpriteRenderer::OnResize()` from `Engine::OnResize()` after forwarding to the Renderer
- **FR-013**: The system MUST use a dynamic D3D11 vertex buffer capable of holding vertex data for up to 1024 sprites per batch (4 vertices × 1024 = 4096 vertices maximum)
- **FR-014**: The system MUST use a static D3D11 index buffer with pre-generated indices for up to 1024 sprite quads (6 indices × 1024 = 6144 indices)
- **FR-015**: The system MUST define a `SpriteVertex` structure with: Position (float2), UV (float2), Color (float4 RGBA)
- **FR-016**: The system MUST define a `Sprite` data structure with: `Texture* texture`, `Vector2 position`, `Vector2 size`, `Vector2 uvMin`, `Vector2 uvMax`, `Color color`
- **FR-017**: The system MUST define a `Vector2` structure with `float x, y` for position, size, and UV coordinates in a new shared header `LongXi/LXEngine/Src/Math/Math.h`
- **FR-018**: The system MUST define a `Color` structure with `float r, g, b, a` for per-sprite color tinting and transparency in `LongXi/LXEngine/Src/Math/Math.h` alongside `Vector2`
- **FR-019**: The system MUST define a vertex shader that transforms screen-space positions into clip space using an orthographic projection matrix
- **FR-020**: The system MUST define a pixel shader that samples the bound texture at the interpolated UV coordinate and multiplies by the interpolated vertex color
- **FR-021**: The system MUST use an orthographic projection matrix with origin at top-left of the window (`[0, width]` × `[0, height]` in screen space mapped to `[-1, 1]` in clip space)
- **FR-022**: The system MUST upload the orthographic projection matrix to the GPU via a D3D11 constant buffer bound to the vertex shader
- **FR-023**: The system MUST batch sprites by texture: all consecutive sprites sharing the same texture are submitted as a single draw call
- **FR-024**: The system MUST automatically flush the current batch when a new texture is encountered during `DrawSprite` calls
- **FR-025**: The system MUST automatically flush the current batch when the sprite count reaches the batch limit (1024 sprites)
- **FR-026**: The system MUST enable alpha blending (source alpha, inverse source alpha blend) for correct transparency rendering
- **FR-027**: The system MUST disable depth writing and depth testing for sprite rendering (sprites are 2D screen-space elements)
- **FR-028**: The system MUST configure a linear texture sampler with clamp address mode for sprite texture sampling
- **FR-029**: The system MUST NOT directly load textures — all textures must originate from `TextureManager`
- **FR-030**: The system MUST call `SpriteRenderer::Begin()` and `SpriteRenderer::End()` from `Engine::Render()` **after** scene rendering and before `EndFrame()` — render order is: `BeginFrame → [future Scene::Render()] → SpriteRenderer::Begin() → [DrawSprite calls] → SpriteRenderer::End() → EndFrame`. Sprites are screen-space overlays and must always appear on top of 3D scene geometry.
- **FR-031**: The system MUST guard `DrawSprite` against null texture pointers — null textures must be skipped with an error log
- **FR-031a**: The system MUST guard all `Begin()`, `DrawSprite()`, and `End()` calls against a disabled state — when SpriteRenderer failed to initialize, all calls are silent no-ops
- **FR-032**: The system MUST guard `DrawSprite` calls outside of an active `Begin()`/`End()` block — such calls must be skipped with an error log
- **FR-033**: The system MUST guard `OnResize` against zero or negative dimensions — such calls must be ignored
- **FR-034**: The system MUST log `[SpriteRenderer] Initialized` after successful initialization; if initialization fails, the system MUST log `[SpriteRenderer] Initialization failed — sprite rendering disabled`
- **FR-035**: The system MUST log `[SpriteRenderer] Batch flushed (N sprites)` each time a batch is flushed, where N is the sprite count in that batch
- **FR-036**: The system MUST log `[SpriteRenderer] Texture bound: [name]` when a new texture is bound during batch rendering
- **FR-037**: The system MUST place `SpriteRenderer`, `Sprite`, `SpriteVertex`, `Vector2`, and `Color` in the `LongXi` namespace (no nested namespaces)
- **FR-038**: The system MUST place SpriteRenderer source files in `LongXi/LXEngine/Src/Renderer/` alongside DX11Renderer; `Vector2` and `Color` MUST be placed in `LongXi/LXEngine/Src/Math/Math.h` as a shared LXEngine math header
- **FR-039**: The system MUST place HLSL shader source as embedded string constants within SpriteRenderer.cpp (no external .hlsl files)
- **FR-040**: The system MUST compile HLSL shaders at runtime during `SpriteRenderer::Initialize()` using D3DCompile

### Key Entities

- **SpriteRenderer**: Engine subsystem that manages GPU pipeline state, sprite batching, and submission to the DX11 device. Owns all GPU resources (shaders, buffers, states). Operates within the Engine subsystem architecture.
- **Sprite**: Lightweight data structure representing a single textured rectangle. Contains texture pointer, screen-space position, size, UV coordinates, and color tint. Not a GPU resource — exists only in CPU memory as a batch buffer entry.
- **SpriteVertex**: GPU vertex structure laid out for Direct3D input assembler. Contains position (float2), UV (float2), and color (float4). Four vertices define one sprite quad.
- **SpriteBatch** (internal): The active accumulation buffer of sprites submitted in the current `Begin()`/`End()` block. Flushed to GPU when texture changes, batch limit is reached, or `End()` is called.
- **Vector2**: Simple 2-component float structure for screen-space positions, pixel sizes, and UV coordinates.
- **Color**: 4-component float RGBA structure for per-sprite color tinting and opacity control.
- **Texture** (existing): Loaded texture object from TextureManager. SpriteRenderer accesses the `ID3D11ShaderResourceView*` from this type for GPU binding.

## Success Criteria

### Measurable Outcomes

- **SC-001**: A single textured sprite renders correctly at the specified screen-space position after one `Begin()`/`DrawSprite()`/`End()` call
- **SC-002**: 32 sprites sharing the same texture produce exactly 1 GPU draw call (verified by log output `Batch flushed (32 sprites)`)
- **SC-003**: 1024 sprites (the batch limit) flush in a single batch without errors; sprite 1025 triggers an automatic mid-frame batch flush and begins a new batch
- **SC-004**: Sprite rendered at `{0, 0}` appears at the top-left pixel of the render target; sprite at `{windowWidth - w, windowHeight - h}` appears at the bottom-right
- **SC-005**: After `Engine::OnResize(newW, newH)`, sprites drawn at `{0, 0}` remain anchored to the top-left corner without repositioning artifacts
- **SC-006**: A sprite drawn with `color = {1.0, 0.0, 0.0, 1.0}` renders visibly red; a sprite with `alpha = 0.5` renders at 50% opacity over the background
- **SC-007**: A sprite drawn with `UVMin = {0.0, 0.0}` and `UVMax = {0.5, 0.5}` renders only the top-left quadrant of the source texture
- **SC-008**: `SpriteRenderer::Initialize()` completes without D3D11 debug layer errors and logs `[SpriteRenderer] Initialized`
- **SC-009**: `Engine::Shutdown()` completes without D3D11 debug layer reporting any live SpriteRenderer GPU resources
- **SC-010**: Adding SpriteRenderer to Engine does NOT require changes to the Application class
- **SC-011**: When SpriteRenderer fails to initialize (e.g., shader compile error), the engine starts successfully and all other subsystems function normally; the log shows `[SpriteRenderer] Initialization failed — sprite rendering disabled`

## Out of Scope

The following items are explicitly OUT of scope for this specification:

- **3D sprites / billboards**: World-space positioning, depth sorting, or camera-facing quads (deferred to future spec)
- **Sprite rotation**: Rotation transforms or arbitrary affine transforms per sprite (deferred to future spec)
- **Sprite scaling from center**: Anchor point control (pivot/origin offset) for non-top-left anchoring (deferred to future spec)
- **Z-ordering**: Depth-sorted sprite rendering or configurable layer ordering (deferred to future spec)
- **Sprite animation**: Animated sprite sheets, frame sequences, or animation controllers (deferred to future spec)
- **Scissor rect / clipping**: Viewport clipping or scissor rectangles for UI panels (deferred to future spec)
- **Multiple render targets**: Off-screen rendering, render-to-texture, or post-processing (deferred to future spec)
- **Instanced rendering**: D3D11 geometry instancing for sprite submission (deferred as optimization)
- **Custom blend modes**: Additive blending, multiply blending, or per-sprite blend state (deferred to future spec)
- **Custom shaders**: User-supplied pixel or vertex shaders for sprite rendering (deferred to future spec)
- **Text rendering**: Font rendering, glyph atlases, or string drawing (deferred to future spec)
- **UI system**: Widget system, layout engine, or interactive UI (deferred to future spec)
- **Particle system**: Particle emitters or particle rendering pipeline (deferred to future spec)
- **Multi-threaded submission**: Background render command generation or deferred contexts (deferred to future spec)
- **Sorting**: Painter's algorithm, back-to-front sorting, or transparency sort (deferred to future spec)

## Assumptions

1. **DX11Renderer access pattern**: SpriteRenderer receives a `DX11Renderer&` reference during initialization and uses it to access the `ID3D11Device*` and `ID3D11DeviceContext*` required to create GPU resources and issue draw calls.
2. **Texture SRV access**: The existing `Texture` class exposes an `ID3D11ShaderResourceView*` accessor (or equivalent) that SpriteRenderer can use to bind textures to the pixel shader.
3. **No math library dependency**: The project does not currently use DirectXMath or a third-party math library. `Vector2` and `Color` are simple POD structs defined in the new shared header `LongXi/LXEngine/Src/Math/Math.h`, making them reusable by future LXEngine subsystems (Camera, Scene, UI) without requiring a separate module. The orthographic projection matrix is computed directly as a 4×4 float array.
4. **HLSL shader compilation at runtime**: Shaders are compiled from embedded string literals using D3DCompile during `SpriteRenderer::Initialize()`. No separate `.hlsl` files, `.cso` compiled shader objects, or offline shader compilation is required for this spec.
5. **D3DCompile availability**: `d3dcompiler.lib` and `D3DCompile` are available in the build environment. If they are not already linked, they must be added to the LXEngine project dependencies.
6. **Batch limit of 1024 sprites**: 1024 sprites per batch is sufficient for Phase 1 use cases (UI, debug overlays, early asset validation). This constant must be named and easy to change in a future spec.
7. **Alpha blending enabled globally**: All sprites use standard alpha blending. There is no per-sprite blend mode selection in this spec.
8. **Depth test and write disabled**: Sprites are rendered in screen space and require no depth testing. The depth-stencil state disables both depth read and depth write.
9. **Linear clamp sampler**: A single shared sampler state with linear filtering and clamp address mode is used for all sprite texture sampling.
10. **Render integration point**: `SpriteRenderer::Begin()` and `End()` are called inside `Engine::Render()` within the `DX11Renderer::BeginFrame()` / `EndFrame()` pair, giving SpriteRenderer access to the bound render target.
11. **Single-threaded only**: Sprite submission and GPU upload occur exclusively on the main thread. No synchronization primitives are required.
12. **Logging macros**: SpriteRenderer uses the existing `LX_ENGINE_INFO` and `LX_ENGINE_ERROR` macros from `LogMacros.h` for debug logging.
13. **File placement**: SpriteRenderer source files are placed in `LongXi/LXEngine/Src/Renderer/` following the established pattern of placing rendering subsystems alongside DX11Renderer.
14. **LXEngine.h update**: `SpriteRenderer.h` must be added to the LXEngine public entry point header (`LXEngine.h`) to maintain the library's single public include surface.
15. **No external test harness**: Testing is manual and visual — the test application in LXShell is updated to draw a sprite, and correctness is verified by running the executable and observing the output.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
