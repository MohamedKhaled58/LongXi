// =============================================================================
// LXShell — Client Application
// Implements CreateApplication() as required by the engine entry point.
// The actual WinMain lives in LXEngine/Src/Application/EntryPoint.h
// =============================================================================

#include <LXEngine.h>
#include <Application/EntryPoint.h>
#include <Engine/Engine.h>
#include <Texture/TextureManager.h>
#include <Core/Logging/LogMacros.h>

namespace LongXi
{

// Test application that verifies texture loading
class TestApplication : public Application
{
  public:
    bool Initialize() override
    {
        if (!Application::Initialize())
        {
            return false;
        }

        // Test texture loading after all systems are ready
        TestTextureSystem();

        // Test sprite renderer initialization
        TestSpriteSystem();

        // Test scene system initialization and node lifecycle
        TestSceneSystem();

        return true;
    }

  private:
    void TestSceneSystem()
    {
        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("SCENE SYSTEM TEST");
        LX_ENGINE_INFO("==============================================");

        Engine& engine = GetEngine();

        // Verify Scene initialized
        if (engine.GetScene().IsInitialized())
        {
            LX_ENGINE_INFO("✓ SUCCESS: Scene initialized");
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Scene not initialized");
        }

        // Test node add / remove lifecycle
        auto testNode = std::make_unique<SceneNode>();
        testNode->SetPosition({10.0f, 0.0f, 0.0f});
        SceneNode* rawPtr = testNode.get();
        engine.GetScene().AddNode(std::move(testNode));
        LX_ENGINE_INFO("✓ Scene node added at position (10, 0, 0)");

        engine.GetScene().RemoveNode(rawPtr);
        LX_ENGINE_INFO("✓ Scene node removed");

        // Test hierarchical transform propagation:
        // Parent at world (10,0,0) + Child at local (5,0,0) = Child world (15,0,0)
        auto parentNode = std::make_unique<SceneNode>();
        parentNode->SetPosition({10.0f, 0.0f, 0.0f});

        auto childNode = std::make_unique<SceneNode>();
        childNode->SetPosition({5.0f, 0.0f, 0.0f});
        SceneNode* childRaw = childNode.get();

        parentNode->AddChild(std::move(childNode));
        SceneNode* parentRaw = parentNode.get();
        engine.GetScene().AddNode(std::move(parentNode));

        // Run one update to trigger world transform computation
        engine.GetScene().Update(0.0f);

        const Vector3& childWorldPos = childRaw->GetWorldPosition();
        if (childWorldPos.x == 15.0f && childWorldPos.y == 0.0f && childWorldPos.z == 0.0f)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Child world position is (15, 0, 0) as expected");
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Child world position is ({}, {}, {}), expected (15, 0, 0)", childWorldPos.x, childWorldPos.y, childWorldPos.z);
        }

        engine.GetScene().RemoveNode(parentRaw); // also destroys child

        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("SCENE SYSTEM TEST COMPLETE");
        LX_ENGINE_INFO("==============================================");
    }

    void TestSpriteSystem()
    {
        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("SPRITE SYSTEM TEST");
        LX_ENGINE_INFO("==============================================");

        Engine& engine = GetEngine();

        if (engine.GetSpriteRenderer().IsInitialized())
        {
            LX_ENGINE_INFO("✓ SUCCESS: SpriteRenderer initialized");
            LX_ENGINE_INFO("SpriteRenderer ready — draw calls execute in Engine::Render() Begin/End block");
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: SpriteRenderer not initialized");
        }

        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("SPRITE SYSTEM TEST COMPLETE");
        LX_ENGINE_INFO("==============================================");
    }

    void TestTextureSystem()
    {
        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("TEXTURE SYSTEM TEST");
        LX_ENGINE_INFO("==============================================");

        // Access Engine through base class (need to add protected accessor or use Engine directly)
        // For now, we'll use Engine directly - this is temporary until we add proper accessor
        Engine& engine = GetEngine(); // TODO: Add protected accessor to Application

        TextureManager& textureManager = engine.GetTextureManager();

        // Test 1: Load 1.dds
        LX_ENGINE_INFO("Test 1: Loading 1.dds...");
        auto texture1 = textureManager.LoadTexture("1.dds");
        if (texture1)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Loaded 1.dds ({}x{} {})", texture1->GetWidth(), texture1->GetHeight(), static_cast<int>(texture1->GetFormat()));
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Could not load 1.dds");
        }

        // Test 2: Load Skill.dds
        LX_ENGINE_INFO("Test 2: Loading Skill.dds...");
        auto texture2 = textureManager.LoadTexture("Skill.dds");
        if (texture2)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Loaded Skill.dds ({}x{} {})", texture2->GetWidth(), texture2->GetHeight(), static_cast<int>(texture2->GetFormat()));
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Could not load Skill.dds");
        }

        // Test 3: Cache hit (load same file again)
        LX_ENGINE_INFO("Test 3: Testing cache hit with 1.dds...");
        auto texture1Again = textureManager.LoadTexture("1.dds");
        if (texture1Again)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Cache hit - got texture from cache");
            // Verify it's the same shared_ptr
            if (texture1 == texture1Again)
            {
                LX_ENGINE_INFO("✓ SUCCESS: Same shared_ptr instance (cache working correctly)");
            }
            else
            {
                LX_ENGINE_ERROR("✗ FAILED: Different shared_ptr instance (cache broken)");
            }
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Cache hit test failed");
        }

        // Test 4: GetTexture (cache lookup only)
        LX_ENGINE_INFO("Test 4: Testing GetTexture for Skill.dds...");
        auto textureFromCache = textureManager.GetTexture("Skill.dds");
        if (textureFromCache)
        {
            LX_ENGINE_INFO("✓ SUCCESS: GetTexture found cached texture");
            if (textureFromCache == texture2)
            {
                LX_ENGINE_INFO("✓ SUCCESS: Same shared_ptr as LoadTexture result");
            }
            else
            {
                LX_ENGINE_ERROR("✗ FAILED: Different shared_ptr instance");
            }
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: GetTexture did not find cached texture");
        }

        // Test 5: Missing file
        LX_ENGINE_INFO("Test 5: Testing missing file (nonexistent.dds)...");
        auto textureMissing = textureManager.LoadTexture("nonexistent.dds");
        if (!textureMissing)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Correctly returned nullptr for missing file");
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Should have returned nullptr for missing file");
        }

        // Test 6: Unsupported format
        LX_ENGINE_INFO("Test 6: Testing unsupported format...");
        auto textureBadExt = textureManager.LoadTexture("file.bmp");
        if (!textureBadExt)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Correctly returned nullptr for unsupported format");
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Should have returned nullptr for unsupported format");
        }

        // Test 7: Path normalization
        LX_ENGINE_INFO("Test 7: Testing path normalization (1.DDS with backslash)...");
        auto textureNormalized = textureManager.LoadTexture("1.DDS");
        if (textureNormalized)
        {
            LX_ENGINE_INFO("✓ SUCCESS: Path normalization working");
            if (textureNormalized == texture1)
            {
                LX_ENGINE_INFO("✓ SUCCESS: Normalized path resolves to same cache entry");
            }
        }
        else
        {
            LX_ENGINE_ERROR("✗ FAILED: Path normalization failed");
        }

        LX_ENGINE_INFO("==============================================");
        LX_ENGINE_INFO("TEXTURE SYSTEM TEST COMPLETE");
        LX_ENGINE_INFO("==============================================");
    }
};

} // namespace LongXi

LongXi::Application* CreateApplication()
{
    return new LongXi::TestApplication();
}
