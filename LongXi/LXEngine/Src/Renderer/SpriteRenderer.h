#pragma once

#include "Math/Math.h"
#include "Renderer/Renderer.h"

#include <memory>

namespace LongXi
{

class Texture;

static constexpr int MAX_SPRITES_PER_BATCH = 1024;

struct SpriteVertex
{
    Vector2 Position;
    Vector2 UV;
    Color Color;
};

class SpriteRenderer
{
public:
    SpriteRenderer();
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    bool Initialize(Renderer& renderer, int width, int height);
    void Shutdown();
    bool IsInitialized() const;

    void OnResize(int width, int height);

    void Begin();
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size);
    void DrawSprite(Texture* texture, Vector2 position, Vector2 size, Vector2 uvMin, Vector2 uvMax, LongXi::Color color);
    void End();

private:
    void FlushBatch();
    void UpdateProjection(int width, int height);

    class Impl;

private:
    Renderer* m_Renderer = nullptr;
    std::unique_ptr<Impl> m_Impl;

    SpriteVertex m_VertexData[MAX_SPRITES_PER_BATCH * 4];

    int m_SpriteCount = 0;
    Texture* m_CurrentTexture = nullptr;

    bool m_Initialized = false;
    bool m_InBatch = false;

    float m_ProjectionMatrix[16] = {};
};

} // namespace LongXi
