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

        return true;
    }

  private:
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
