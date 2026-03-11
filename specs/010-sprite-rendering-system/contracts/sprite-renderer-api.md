# Contract: SpriteRenderer Public API

**Feature**: 010-sprite-rendering-system
**Namespace**: `LongXi`
**Header**: `LongXi/LXEngine/Src/Renderer/SpriteRenderer.h`

---

## Math Types Contract

**Header**: `LongXi/LXEngine/Src/Math/Math.h`

```cpp
namespace LongXi
{

struct Vector2
{
    float x;
    float y;
};

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

} // namespace LongXi
```

---

## SpriteRenderer Class Contract

```cpp
namespace LongXi
{

class DX11Renderer;
class Texture;

class SpriteRenderer
{
  public:
    SpriteRenderer();
    ~SpriteRenderer();

    // Non-copyable, non-movable
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Initialize GPU pipeline state.
    // Returns false if any GPU resource creation fails (graceful — engine continues).
    // width/height: initial viewport dimensions for orthographic projection.
    bool Initialize(DX11Renderer& renderer, int width, int height);

    // Release all GPU resources. Safe to call even if Initialize() failed.
    void Shutdown();

    // Returns true if Initialize() succeeded.
    bool IsInitialized() const;

    // =========================================================================
    // Resize
    // =========================================================================

    // Update orthographic projection matrix to new viewport dimensions.
    // Ignored if width <= 0 || height <= 0.
    // Must be called from Engine::OnResize().
    void OnResize(int width, int height);

    // =========================================================================
    // Frame Rendering
    // =========================================================================

    // Begin a sprite submission block. Resets the batch buffer.
    // Must be called before any DrawSprite() calls.
    // No-op if not initialized or already in batch.
    void Begin();

    // Draw a textured quad at screen-space position with default UV (full texture) and white color.
    // position: top-left pixel coordinate (origin = window top-left)
    // size: width and height in pixels
    // No-op if texture is null, size is degenerate, or not in a Begin()/End() block.
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size);

    // Draw a textured quad with explicit UV sub-region and color tint.
    // uvMin: top-left UV coordinate (default: {0,0})
    // uvMax: bottom-right UV coordinate (default: {1,1})
    // color: RGBA tint multiplied per pixel (default: {1,1,1,1} = no tint)
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size,
                    Vector2 uvMin, Vector2 uvMax, Color color);

    // Flush all pending sprites to the GPU and end the submission block.
    // No-op if not in a Begin()/End() block.
    void End();
};

} // namespace LongXi
```

---

## Engine Accessor Contract

```cpp
// In Engine class (Engine.h):
SpriteRenderer& GetSpriteRenderer();
```

**Pre-condition**: `Engine::IsInitialized()` must be true before calling `GetSpriteRenderer()`.

---

## DX11Renderer Extension Contract

Two public accessor methods added to `DX11Renderer`:

```cpp
// In DX11Renderer class (DX11Renderer.h):
ID3D11Device*        GetDevice()  const;  // Returns raw device pointer (not transferred)
ID3D11DeviceContext* GetContext() const;  // Returns raw context pointer (not transferred)
```

**Ownership**: Callers do NOT take ownership. Lifetime is managed by DX11Renderer.

---

## Batch Behavior Contract

| Condition | Result |
|-----------|--------|
| DrawSprite() called with same texture as current batch | Append to current batch |
| DrawSprite() called with different texture | Flush current batch, start new batch with new texture |
| DrawSprite() called when `m_SpriteCount == MAX_SPRITES_PER_BATCH` | Flush current batch, start new batch |
| End() called | Flush remaining batch (if m_SpriteCount > 0), reset state |
| Begin() called | Reset: m_SpriteCount=0, m_CurrentTexture=null, m_InBatch=true |

**Constant**: `MAX_SPRITES_PER_BATCH = 1024`

---

## Error Handling Contract

| Condition | Behavior |
|-----------|----------|
| Initialize() GPU resource fails | Returns false, SpriteRenderer disabled, engine continues |
| DrawSprite() with null texture | Skipped, `LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite: null texture")` |
| DrawSprite() outside Begin()/End() | Skipped, `LX_ENGINE_ERROR("[SpriteRenderer] DrawSprite called outside Begin/End")` |
| OnResize() with width<=0 or height<=0 | Ignored silently |
| Any call when disabled | Silent no-op |

---

## Render Integration Contract

**Called from `Engine::Render()`** — render order:

```
DX11Renderer::BeginFrame()
  → [future Scene::Render()]
  → SpriteRenderer::Begin()
      → [DrawSprite calls from game/test code]
  → SpriteRenderer::End()
DX11Renderer::EndFrame()
```

**Called from `Engine::OnResize()`**:
```
DX11Renderer::OnResize(width, height)
SpriteRenderer::OnResize(width, height)
```

---

## Logging Contract

| Event | Log Message |
|-------|-------------|
| Initialize success | `[SpriteRenderer] Initialized` |
| Initialize failure | `[SpriteRenderer] Initialization failed — sprite rendering disabled` |
| Batch flush | `[SpriteRenderer] Batch flushed (N sprites)` |
| Texture bind | `[SpriteRenderer] Texture bound` |
| Shutdown | `[SpriteRenderer] Shutdown` |

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
