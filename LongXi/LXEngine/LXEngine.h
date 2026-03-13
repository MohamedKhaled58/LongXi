#pragma once

// =============================================================================
// LXEngine Public Entry Point
// Engine/runtime systems library
// =============================================================================

#include "Animation/AnimationClip.h"
#include "Animation/AnimationPlayer.h"
#include "Application/Application.h"
#include "Assets/C3/C3RuntimeLoader.h"
#include "Assets/C3/Resources.h"
#include "Assets/C3/RuntimeMesh.h"
#include "Assets/ResourceManager.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Math/Math.h"
#include "Core/Profiling/ProfileScope.h"
#include "Core/Timing/TimingService.h"
#include "Engine/Engine.h"
#include "Hero/LXHero.h"
#include "LXGameMap.h"
#include "Profiling/ProfilerCollector.h"
#include "Renderer/LXHeroRenderer.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Texture/TextureManager.h"
#include "Window/Win32Window.h"

// Entry point: include Application/EntryPoint.h ONCE in the client executable
