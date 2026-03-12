#pragma once

// =============================================================================
// LXEngine Public Entry Point
// Engine/runtime systems library
// =============================================================================

#include "Application/Application.h"
#include "Engine/Engine.h"
#include "Window/Win32Window.h"
#include "Texture/TextureManager.h"
#include "Renderer/SpriteRenderer.h"
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Math/Math.h"
#include "Timing/TimingService.h"
#include "Profiling/ProfilerCollector.h"
#include "Profiling/ProfileScope.h"
#include "LXGameMap.h"
#include "Core/Logging/LogMacros.h"

// Entry point: include Application/EntryPoint.h ONCE in the client executable
