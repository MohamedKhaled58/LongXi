#pragma once

// =============================================================================
// LXEngine Public Entry Point
// Engine/runtime systems library
// =============================================================================

#include "Animation/AnimationClip.h"
#include "Animation/AnimationPlayer.h"
#include "Application/Application.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/Engine.h"
#include "LXGameMap.h"
#include "Math/Math.h"
#include "Profiling/ProfileScope.h"
#include "Profiling/ProfilerCollector.h"
#include "Renderer/SpriteRenderer.h"
#include "Resource/C3RuntimeLoader.h"
#include "Resource/RuntimeMesh.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Texture/TextureManager.h"
#include "Timing/TimingService.h"
#include "Window/Win32Window.h"

// Entry point: include Application/EntryPoint.h ONCE in the client executable
