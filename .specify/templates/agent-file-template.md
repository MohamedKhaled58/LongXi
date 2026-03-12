# [PROJECT NAME] Development Guidelines

Auto-generated from all feature plans. Last updated: [DATE]

## Active Technologies

[EXTRACTED FROM ALL PLAN.MD FILES]

## Project Structure

```text
[ACTUAL STRUCTURE FROM PLANS]
```

## Commands

[ONLY COMMANDS FOR ACTIVE TECHNOLOGIES]

## Code Style

[LANGUAGE-SPECIFIC, ONLY FOR LANGUAGES IN USE]

## Recent Changes

[LAST 3 FEATURES AND WHAT THEY ADDED]

<!-- MANUAL ADDITIONS START -->
## Architectural Rules

**IMPORTANT:** This repository has comprehensive architectural rules defined in CLAUDE.md. Key rules include:

- **Layered Architecture:** LXShell (Debug UI) → LXEngine (Systems) → LXCore (Foundation)
- **Debug UI Rules:**
  - DebugUI lives in LXShell (NOT in LXEngine)
  - ImGuiLayer is the ONLY LXShell class that includes `<d3d11.h>`
  - LXEngine MUST NOT include ImGui or any UI framework headers
  - ImGui is ONLY for Debug/Dev builds (never in LX_DIST)
- **GPU State:** All GPU state is owned by LXEngine; Shell receives opaque `void*` handles
- **Renderer Abstraction:** DX11Renderer implements backend-agnostic Renderer base class
- **ExecuteExternalPass:** Shell rendering (ImGui) happens through Engine callback mechanism
- **Naming:** Private members use `m_PascalCase`, flat `LongXi` namespace (no nesting)
- **All source files** are under `Src/` subdirectory of each module

**CRITICAL:** When in doubt, consult CLAUDE.md for complete architectural rules.

<!-- MANUAL ADDITIONS END -->

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
