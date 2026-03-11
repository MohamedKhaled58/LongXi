# Implementation Plan: Win32 Shell Bootstrap

**Branch**: `001-win32-shell-bootstrap` | **Date**: 2026-03-10 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-win32-shell-bootstrap/spec.md`

## Summary

Establish a bootable Windows-native client shell for the Long Xi Modernized Clone with controlled application lifecycle, debug console, Win32 window creation, message pump, and clean shutdown. This specification defines the foundational executable entry point and central `Application` class that will coordinate future subsystem integration (rendering, input, resources, networking). The implementation focuses on thin entrypoint discipline, centralized lifecycle ownership, and raw Win32 API usage without abstraction frameworks.

## Technical Context

**Language/Version**: C++23
**Primary Dependencies**: Spdlog (vendor dependency for logging), Windows SDK (Win32 API)
**Storage**: N/A (no data persistence in this spec)
**Testing**: Manual verification via build execution, window creation, and shutdown observation
**Target Platform**: Windows x64 (Windows-only, per constitution)
**Project Type**: Desktop application (native Windows executable)
**Performance Goals**: Window appears within 2 seconds, shutdown within 1 second, 100% uptime during message pump operation
**Constraints**: Single-threaded runtime (Phase 1), <20 lines in WinMain, static library architecture (LXShell → LXEngine → LXCore), WinMain must delegate to Application class
**Scale/Scope**: Single executable, single window, single instance (no multiplayer), foundation infrastructure only

## Constitution Check

_GATE: Must pass before Phase 0 research. Re-check after Phase 1 design._

### Initial Gate Assessment

**Article III: Non-Negotiable Technical Baseline** - ✅ COMPLIANT

- Platform: Windows-only ✅
- Windowing: Raw Win32 API ✅
- Build System: Premake5 generating Visual Studio 2026 projects ✅
- Toolchain: MSVC v145, C++23 ✅
- Module Structure: LXShell → LXEngine → LXCore ✅
- Dependency direction: LXShell → LXEngine → LXCore ✅
- Static libraries only ✅

**Article IV: Architectural Laws** - ✅ COMPLIANT

- Entrypoints remain thin ✅ (FR-001, FR-010, AC-007)
- Module boundaries respected ✅ (LXShell executable, LXEngine/LXCore static libraries)
- No god modules ✅ (Application class has focused 3-method interface)
- No hidden global state ✅ (lifecycle centered in Application class)
- Platform/rendering separation ✅ (no rendering in this spec)
- Convenience doesn't override architecture ✅ (simple Win32 approach)

**Article IX: Threading and Runtime Discipline** - ✅ COMPLIANT

- Phase 1 runtime is single-threaded ✅ (explicitly specified)
- Architecture remains MT-aware ✅ (no design choices that would prevent future multithreading)

**Article XII: Phase 1 Constitutional Guardrails** - ✅ COMPLIANT

- Native client foundation ✅ (within scope)
- Shell/bootstrap ✅ (within scope)
- Logging/debugging ✅ (Spdlog integration)
- Win32 window ✅ (within scope)
- Debug console ✅ (AllocConsole-based)
- DX11 initialization ✅ (explicitly out of scope)
- Full gameplay ✅ (explicitly out of scope)
- Full multiplayer ✅ (explicitly out of scope)

### Gate Result: ✅ PASS - No constitutional violations

All requirements align with constitutional constraints. The specification is narrow and focused on Phase 1 foundation infrastructure.

## Project Structure

### Documentation (this feature)

```text
specs/001-win32-shell-bootstrap/
├── plan.md              # This file
├── research.md          # Phase 0 output (best practices, Win32 patterns)
├── data-model.md        # Phase 1 output (Application class, entities)
├── quickstart.md        # Phase 1 output (build/run instructions)
├── contracts/           # Phase 1 output (if applicable)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
LongXi/
├── LXCore/
│   └── Src/              # Low-level shared foundation
│       ├── External/     # Third-party dependencies (Spdlog)
│       ├── Core/         # Core utilities, logging setup
│       └── Platform/     # Platform abstractions (if needed)
├── LXEngine/
│   └── Src/              # Engine/runtime systems
│       ├── Application/  # Application lifecycle class
│       ├── Window/       # Win32 window wrapper
│       └── Logging/      # Logging macros (LX_ENGINE_INFO, etc.)
└── LXShell/
    └── Src/              # Executable entry point
        └── main.cpp     # WinMain entrypoint (<20 lines, only file in LXShell for this spec)
```

**Structure Decision**: This feature uses the existing LongXi modular structure defined by the constitution. LXCore provides foundation utilities and logging infrastructure, LXEngine contains the Application class and window management, LXShell serves as the thin executable entry point. The dependency direction LXShell → LXEngine → LXCore is maintained.

## Complexity Tracking

> **No constitutional violations to track - specification is compliant and narrow.**

This specification intentionally minimizes complexity by:

- Focusing on bootstrap foundation only (no rendering, gameplay, networking)
- Using raw Win32 API directly (no abstraction framework overhead)
- Simple 3-method Application interface (Initialize/Run/Shutdown)
- Standard Win32 message pump (no custom event systems)
- Single-threaded execution (no threading complexity)

## Phase 0: Research & Best Practices

### Research Tasks

**Task 1: Win32 Application Foundation Patterns**

- Research: Standard Win32 application lifecycle patterns
- Research: Window class registration and creation best practices
- Research: Message pump implementation patterns (GetMessage/DispatchMessage)
- Research: AllocConsole usage and stream redirection patterns
- Decision: Use standard Win32 patterns - window class registration, CreateWindowEx, message loop with GetMessage/DispatchMessage, proper window procedure handling

**Task 2: Spdlog Integration for Native Applications**

- Research: Spdlog integration patterns for native Win32 applications
- Research: Console sink configuration for colored output
- Research: Custom macro creation (LX_INFO, LX_CORE_INFO, LX_ENGINE_INFO)
- Decision: Use Spdlog's stdout_color_sink or custom console sink, create module-specific macros following established C++ logging practices

**Task 3: Application Class Design Patterns**

- Research: RAII patterns for resource management in Application class
- Research: Clean separation between initialization, runtime, and shutdown phases
- Research: Error handling strategies for window creation failure
- Decision: Constructor doesn't create window, Initialize() handles all setup, Run() owns message pump, Shutdown() handles teardown

**Task 4: Build System Integration**

- Research: Premake5 configuration for Win32 applications
- Research: Proper linking with Windows SDK libraries
- Research: Spdlog include path configuration
- Decision: Use existing Premake5 setup, add Windows SDK links, configure Spdlog include through existing Vendor/Spdlog/include path

### Research Findings Summary

All NEEDS CLARIFICATION items from technical context have been resolved through research:

| Topic               | Decision                                     | Rationale                                                                                 |
| ------------------- | -------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Win32 API Usage     | Raw Win32 API (no frameworks)                | Constitutional requirement, provides direct control, minimal overhead                     |
| Message Pump        | Standard GetMessage/DispatchMessage loop     | Idiomatic Win32 approach, well-documented, handles shutdown cleanly                       |
| Console Allocation  | AllocConsole (always allocate)               | Simpler than AttachConsole, guarantees console availability, deterministic behavior       |
| Application Design  | 3-method interface (Initialize/Run/Shutdown) | Clean separation of lifecycle phases, supports constitutional requirements                |
| Logging Integration | Spdlog with custom macros                    | Provides structured logging, validates vendor dependency, supports colored console output |
| Error Handling      | Controlled failure with logging              | Meets requirement (FR-009), prevents undefined behavior, aids debugging                   |

## Phase 1: Design & Contracts

### Data Model

**Application Class Entity**

See `data-model.md` for authoritative Application class design including:

- Three lifecycle methods: Initialize(), Run(), Shutdown()
- Private members: m_WindowHandle, m_ShouldShutdown, m_Initialized
- Static WindowProc for Win32 message handling
- Helper methods: CreateConsoleWindow(), InitializeLogging(), CreateMainWindow(), DestroyMainWindow()
  (fixes I1 - removed duplicate inline definition, canonicalized data-model.md as authoritative source)

**Window Entity**

See `data-model.md` for authoritative Win32Window class design including:

- RAII wrapper around Win32 window handle
- Create(), Show(), Destroy() methods
- Window properties: title ("LongXi"), size (1024x768), style (WS_OVERLAPPEDWINDOW)

**Logging Macros (Custom per Spec Clarification)**

```cpp
// LXCore/Src/Core/Logging/LogMacros.h
#define LX_CORE_INFO(...)    // LXCore logging
#define LX_ENGINE_INFO(...)   // LXEngine logging
#define LX_INFO(...)         // LXShell logging
// Future: LX_RENDER_INFO, LX_RESOURCE_INFO, etc.
```

### Interface Contracts

**Public Entry Point Headers (CLAUDE.md Section 18)**

```cpp
// LXCore/LXCore.h - Public entry point for LXCore library
#pragma once
#include "Core/Logging/LogMacros.h"
// Other public LXCore exports...
```

```cpp
// LXEngine/LXEngine.h - Public entry point for LXEngine library
#pragma once
#include "Application/Application.h"
#include "Window/Win32Window.h"
#include "Core/Logging/LogMacros.h"
// Other public LXEngine exports...
```

**Application Lifecycle Contract**

The `Application` class provides a three-phase lifecycle contract:

1. **Initialize() Phase**:
   - **Precondition**: Application object constructed, no resources allocated
   - **Action**: Allocates debug console (development builds), initializes Spdlog, creates Win32 window
   - **Postcondition**: Window created and visible, logging functional
   - **Returns**: true on success, false on failure
   - **Failure Mode**: Logs error, returns false, caller should not call Run() or Shutdown()

2. **Run() Phase**:
   - **Precondition**: Initialize() succeeded, window is valid
   - **Action**: Enters message pump (GetMessage/DispatchMessage loop), processes Win32 messages
   - **Postcondition**: Message pump exited, resources still valid
   - **Returns**: Exit code (typically 0)
   - **Termination**: Exits when WM_QUIT message received via PostQuitMessage

3. **Shutdown() Phase**:
   - **Precondition**: Run() has completed, window and resources are valid
   - **Action**: Destroys window, deallocates console (if allocated), flushes logs
   - **Postcondition**: All resources released, process ready to exit
   - **Returns**: void

**Error Handling Contract**

- Window creation failure: Returns false from Initialize(), logs error, process exits cleanly
- Message pump failure: Logs error, posts quit message, exits gracefully
- Shutdown failure: Logs error, best-effort cleanup, process terminates

### Quickstart Guide

**Building the Solution**

1. **Prerequisites**:
   - Visual Studio 2026 with MSVC v145 toolchain
   - Windows SDK installed
   - Premake5 in PATH (or use Vendor/Bin/premake5.exe)

2. **Generate Project Files**:

   ```batch
   Win-Generate Project.bat
   ```

3. **Build Solution**:

   ```batch
   Win-Build Project.bat Debug x64
   ```

4. **Run the Application**:
   ```batch
   Build/Debug/Executables/LXShell.exe
   ```

**Expected Behavior**

1. **Startup**:
   - Debug console appears
   - "Application starting..." logged via LX_INFO
   - Window titled "LongXi" appears (1024x768)
   - "Application initialized successfully" logged

2. **Runtime**:
   - Window remains open and responsive
   - Message pump processes Win32 messages
   - Application stays running until close requested

3. **Shutdown** (X button or Alt-F4):
   - "Application shutting down..." logged via LX_INFO
   - Window closes
   - Debug console remains visible (for log review)
   - Process exits cleanly

**Troubleshooting**

- **Build errors**: Check Windows SDK installation, verify Premake5 generation
- **Window not appearing**: Check debug console for error messages, verify Initialize() return value
- **Hang on shutdown**: Check for infinite loops in Run() method, verify WM_QUIT message posting
- **Console not visible**: Verify development build configuration, check AllocConsole success

## Design Validation

### Post-Design Constitution Check

**Re-evaluation after Phase 1 design** - ✅ STILL COMPLIANT

All design decisions maintain constitutional compliance:

- Application class design maintains thin entrypoint discipline ✅
- Three-method interface preserves clear lifecycle ownership ✅
- Static library structure (LXShell → LXEngine → LXCore) maintained ✅
- Single-threaded execution respected ✅
- No premature integration or scope creep ✅

### Complexity Assessment

No additional complexity introduced. The design remains intentionally simple:

- **Application class**: 3 public methods, focused responsibility
- **Window class**: RAII wrapper around Win32 HWND
- **Logging**: Spdlog integration (vendor dependency, not new complexity)
- **Message pump**: Standard Win32 pattern (GetMessage/DispatchMessage)

### Architecture Alignment

**CLAUDE.md Compliance:**

- **Section 16 (Naming)**: Private members use m_PascalCase ✅
- **Section 17 (Third-Party Dependency)**: Spdlog is sole external dependency, defined in Vendor/ ✅
- **Section 18 (Library Public Entry Points)**: LXCore.h and LXEngine.h provide clean public interfaces ✅
- **Section 19 (External Includes)**: Spdlog includes clearly separated in External/ ✅

## Implementation Sequence

The implementation will proceed in this order:

1. **LXCore Foundation**:
   - Setup Spdlog integration
   - Define custom logging macros (LX_CORE_INFO)
   - Create LXCore.h public entry point

2. **LXEngine Application**:
   - Implement Application class with Initialize/Run/Shutdown
   - Implement Win32Window wrapper
   - Define custom logging macros (LX_ENGINE_INFO)
   - Create LXEngine.h public entry point

3. **LXShell Executable**:
   - Implement WinMain (thin, <20 lines)
   - Define custom logging macros (LX_INFO)
   - Wire up Application lifecycle

4. **Build Integration**:
   - Update Premake5 configuration for Win32 linking
   - Configure include paths for Spdlog
   - Test build across all configurations (Debug, Release, Dist)

## Next Steps

After this plan is completed, the next phase is:

**Phase 2: Task Generation** - Execute `/speckit.tasks` to generate actionable task breakdown for implementation.

This plan provides:

- ✅ Clear technical context with all NEEDS CLARIFICATION resolved
- ✅ Constitutional compliance verified (both initial and post-design)
- ✅ Concrete data model for Application and Window classes
- ✅ Interface contracts defined for library public entry points
- ✅ Quickstart guide for building and running
- ✅ Research findings consolidating all technical decisions

The specification is ready for implementation planning and task breakdown.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
