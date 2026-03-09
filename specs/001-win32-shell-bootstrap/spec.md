# Spec 001 — Win32 Shell Bootstrap

**Feature Branch**: `001-win32-shell-bootstrap`
**Created**: 2026-03-09
**Status**: Accepted
**Input**: User description: "Spec 001 Win32 Shell Bootstrap"

## User Scenarios & Testing _(mandatory)_

### User Story 1 - Application Bootstrap (Priority: P1)

As a developer, I need a bootable Windows-native client shell so that I have a controlled application lifecycle into which I can integrate later systems (rendering, input, resources, networking).

**Why this priority**: Without a bootable shell and controlled lifecycle, no other systems can be safely integrated. This is foundation infrastructure.

**Independent Test**: The executable launches, creates a Win32 window, maintains a message pump, and exits cleanly when the window is closed. Startup and shutdown are observable via console output.

**Acceptance Scenarios**:

1. **Given** the system is in a clean state, **When** I launch LXShell.exe, **Then** a debug console is available and a Win32 window appears
2. **Given** the application is running, **When** I close the window via the X button or Alt-F4, **Then** the application exits cleanly with no crash or hanging
3. **Given** the application fails to create the window, **Then** a controlled error message is logged and the process exits cleanly
4. **Given** the application is initializing, **When** WM_CLOSE is received during initialization, **Then** the application logs the event, cancels initialization, and exits cleanly (fixes U1 - edge case acceptance scenario)
5. **Given** multiple instances are launched simultaneously, **When** they run concurrently, **Then** each instance operates independently without interference (fixes U1 - edge case acceptance scenario)

---

### Edge Cases

- What happens when window creation fails due to insufficient system resources?
- What happens when the application receives WM_CLOSE during initialization?
- What happens when multiple instances are launched simultaneously (for now, multiple instances allowed)?

## Clarifications

### Session 2026-03-10

- Q: What logging mechanism should be used for startup/shutdown observability? → A: Spdlog integration with custom logging macros (LX_CORE_INFO, LX_ENGINE_INFO, LX_INFO) with equal spacing and colored simple console logging
- Q: What interface methods should the Application class expose? → A: Core lifecycle methods (Initialize/Run/Shutdown) - Initialize handles debug console setup, logging initialization, Win32 window creation; Run owns main runtime loop and Win32 message pump; Shutdown handles orderly teardown and controlled exit
- Q: What are the concrete window properties for the bootstrap Win32 window? → A: Standard Win32 desktop window with title "LongXi", WS_OVERLAPPEDWINDOW style, 1024x768 size, shown in normal mode with resize support; fullscreen and persisted properties are out of scope
- Q: What debug console approach should be used? → A: AllocConsole (always allocate) - Explicitly allocate debug console during bootstrap in development builds for deterministic diagnostics; console availability independent of parent process; includes stream redirection for stdout/stderr as needed
- Q: How does the Application::Run() method know when to exit? → A: Standard Win32 message-driven shutdown - WM_CLOSE leads to window destruction, WM_DESTROY posts quit message via PostQuitMessage(0), Application::Run() exits when message pump receives quit condition; cross-thread or multi-source shutdown signaling is out of scope

## Requirements _(mandatory)_

### Functional Requirements

- **FR-001**: The solution SHALL provide a thin `LXShell` executable entrypoint with <20 lines of code that delegates immediately to a central `Application` class, keeping all lifecycle logic out of `WinMain` (consolidates FR-001, FR-010, FR-011)
- **FR-002**: LXShell SHALL create and run a central `Application` object that exposes exactly three lifecycle methods:
  - `Initialize()`: Allocates debug console, initializes Spdlog logging, creates Win32 window
  - `Run()`: Owns the main runtime loop and Win32 message pump until shutdown is requested
  - `Shutdown()`: Handles orderly teardown of bootstrap resources and controlled application exit
    (consolidates FR-002, FR-014, FR-015, FR-016)
- **FR-003**: The application SHALL allocate a debug console using AllocConsole in Debug configuration builds for startup/shutdown visibility (Release and Dist configurations do not allocate console; fixes U3 - clarified "development builds")
- **FR-004**: Console stream redirection SHALL redirect stdout and stderr to the allocated console window (fixes A1 - removed "as needed" vagueness)
- **FR-005**: The application SHALL own a stable message pump (GetMessage/DispatchMessage loop) that processes Win32 messages until GetMessage returns false due to a quit message from PostQuitMessage (consolidates FR-005, FR-006, FR-021)
- **FR-006**: The application SHALL support orderly shutdown through normal window close behavior where the window procedure handles WM_CLOSE and WM_DESTROY as part of the shutdown signaling path (consolidates FR-007, FR-020)
- **FR-007**: The solution SHALL integrate Spdlog for structured logging with custom module-specific macros (LX_INFO for shell, LX_CORE_INFO for core, LX_ENGINE_INFO for engine) with fixed-width module prefixes (`[CORE]`, `[ENGINE]`, `[SHELL]`) for aligned colored console output (consolidates FR-008, FR-013; fixes A3 - clarified "equal spacing")
- **FR-008**: Window creation failure SHALL result in controlled failure with error logging and clean process exit, not undefined behavior or crashes
- **FR-009**: The message pump SHALL be stable and SHALL NOT re-enter the main loop during shutdown
- **FR-010**: The application SHALL create a native Win32 window using raw Win32 API calls with title "LongXi", WS_OVERLAPPEDWINDOW style, and 1024x768 initial size
- **FR-011**: The window SHALL be shown in normal windowed mode on launch (not fullscreen, not maximized, not minimized)
- **FR-012**: The window SHALL support standard resize behavior as part of WS_OVERLAPPEDWINDOW style

### Key Entities

- **LXShell**: Executable entrypoint (thin wrapper)
- **Application**: Central lifecycle owner with three core methods:
  - `Initialize()`: Debug console setup, logging initialization, Win32 window creation
  - `Run()`: Main runtime loop and Win32 message pump ownership
  - `Shutdown()`: Orderly teardown of bootstrap resources and controlled exit
- **Win32 Window**: Native window created via CreateWindowEx with title "LongXi", WS_OVERLAPPEDWINDOW style, 1024x768 initial size, shown in normal mode
- **Message Pump**: Win32 message loop (GetMessage/DispatchMessage) that exits on quit message from PostQuitMessage
- **Shutdown Signaling**: Standard Win32 message-driven shutdown path (WM_CLOSE → WM_DESTROY → PostQuitMessage → message pump exit)
- **Logging System**: Spdlog integration with custom macros (LX_INFO, LX_CORE_INFO, LX_ENGINE_INFO) for colored console output

## Success Criteria _(mandatory)_

### Measurable Outcomes

- **SC-001**: Solution builds successfully in Debug, Release, and Dist configurations
- **SC-002**: Executable launches successfully and creates a visible Win32 window within 2 seconds
- **SC-003**: Debug console is available at launch and shows startup sequence
- **SC-004**: Application remains running in a stable message pump until explicitly closed
- **SC-005**: Window close action (X button, Alt-F4) results in clean process exit within 1 second
- **SC-006**: Startup and shutdown events are observable via console output with clear sequencing
- **SC-007**: Lifecycle ownership is centered in `Application` class, with `WinMain` containing <20 lines of code
- **SC-008**: Window creation failure produces logged error and clean exit, no crash or undefined behavior
- **SC-009**: Application SHALL NOT crash, hang, or become unresponsive during normal message pump operation (fixes A2 - replaced vague "100% uptime" with measurable behavior)
- **SC-010**: Code review confirms thin entrypoint, centralized lifecycle ownership, and absence of premature integration

## Constraints _(mandatory)_

### Platform Constraints

- Windows-only (no cross-platform abstraction at this stage)
- Raw Win32 API for windowing (no frameworks like SDL, GLFW, or WinRT)
- MSVC v145 toolchain, C++23 standard
- Static library architecture: LXShell → LXEngine → LXCore

### Architectural Constraints

- Central `Application` class owns lifecycle
- Subsystem-oriented architecture (not layer-stack-first)
- Single-threaded runtime for this stage (Phase 1)
- Debug console attached during development

### Constitutional Constraints

This specification is governed by the Long Xi Modernized Clone Constitution v1.0.0 and must comply with:

- Article III (Non-Negotiable Technical Baseline)
- Article IV (Architectural Laws)
- Article IX (Threading and Runtime Discipline)
- Article XII (Phase 1 Constitutional Guardrails)

## Out of Scope _(mandatory)_

The following are explicitly OUT OF SCOPE for this specification:

- Cross-thread or multi-source shutdown signaling mechanisms
- Console attachment behavior (AttachConsole) or configurable console strategies
- Window management features beyond basic bootstrap (fullscreen mode, window position persistence, multi-monitor support)
- DirectX 11 initialization or rendering
- Input system implementation beyond what Win32 message processing naturally requires
- Resource/filesystem layer
- Asset loading or parsing
- Animation systems
- Map loading
- Gameplay systems
- Networking implementation
- Multithreaded runtime execution
- Plugin/DLL architecture
- Dedicated server components
- Editor or tool interfaces

## Deliverables

### Concrete Deliverables

- Bootable `LXShell.exe` executable
- `Application` lifecycle class with three core methods: Initialize(), Run(), Shutdown()
- AllocConsole-based debug console allocation for development builds
- Console stream redirection setup for stdout/stderr
- Spdlog integration with custom logging macros (LX_INFO, LX_CORE_INFO, LX_ENGINE_INFO)
- Win32 window creation and registration (1024x768, WS_OVERLAPPEDWINDOW, "LongXi" title)
- Message pump implementation with quit-message exit condition
- Controlled shutdown path (WM_CLOSE → WM_DESTROY → PostQuitMessage → message pump exit)
- Startup/shutdown logging with colored console output
- WinMain entrypoint (thin, <20 lines)

## Acceptance Criteria _(mandatory)_

### Concrete Acceptance Criteria

- **AC-001**: `LXShell.exe` builds without errors or warnings in all three configurations (Debug, Release, Dist)
- **AC-002**: Launching `LXShell.exe` produces a visible Win32 window titled "LongXi" with 1024x768 size in normal windowed mode
- **AC-003**: Debug console window is visible at launch and shows startup and shutdown log entries (fixes C1 - removed exact string requirement, focuses on observable behavior)
- **AC-004**: Application window remains open and responsive until explicitly closed
- **AC-005**: Closing the window via X button, Alt-F4, or programmatic PostQuitMessage results in clean process exit
- **AC-006**: After shutdown, no child processes remain and the process exits with code 0 (fixes A4 - replaced Process Monitor tool reference with behavior specification)
- **AC-007**: `WinMain` function contains <20 lines of code (excluding comments and blank lines)
- **AC-008**: Code review confirms absence of DX11, input, resource, or networking integration
- **AC-009**: Code review confirms compliance with constitutional requirements for thin entrypoint and centralized lifecycle (consolidates AC-010, absorbs AC-008's lifecycle logic check)

## Risks

### Technical Risks

- **Risk 1**: Putting too much logic in `WinMain` violates thin-entrypoint discipline
  - _Mitigation_: Code review requirement and line count limit (<20 lines)

- **Risk 2**: Allowing `LXShell` to become thick with game/engine logic
  - _Mitigation_: Architectural enforcement and explicit out-of-scope declarations

- **Risk 3**: Coupling shell bootstrap too early to future renderer/resource systems
  - _Mitigation_: Strict scope boundary and code review for premature integration

- **Risk 4**: Unclear shutdown ownership leading to crashes or hangs on exit
  - _Mitigation_: Explicit shutdown sequence in `Application` class and WM_CLOSE handling

- **Risk 5**: Uncontrolled process exit paths (exit() calls, unhandled exceptions)
  - _Mitigation_: Centralized exit through `Application` shutdown, no direct exit() calls

## Assumptions

- Later specifications will add DX11 bootstrap as a separate subsystem
- Later specifications will define input, resource, and rendering systems in detail
- Current goal is executable shell establishment, not feature completeness
- Single-threaded execution is sufficient for this stage (per constitutional Phase 1 requirements)
- Debug console allocation is acceptable for development builds
- Multiple application instances are allowed at this stage (no single-instance enforcement)

## Open Questions

_No unresolved questions at this time._

## Implementation Notes (Post-Acceptance)

The following architectural decisions were made during implementation and deviate from or extend the original specification:

1. **Hazel-Style Entry Point**: WinMain is defined in `LXEngine/Src/Application/EntryPoint.h`, not in LXShell. The shell only implements `LongXi::CreateApplication()`. This keeps the entry point in the engine where lifecycle orchestration belongs.
2. **Consolidated Logging Macros**: All logging macros (LX*CORE*_, LX*ENGINE*_, LX\_\*) are defined in a single `LXCore/Src/Core/Logging/LogMacros.h` to avoid include path collisions between modules.
3. **Separate Log Class**: Spdlog initialization lives in `LongXi::Log` (LXCore), not in `Application`. `Log::Initialize()` and `Log::Shutdown()` are called from `EntryPoint.h`.
4. **LongXi Namespace**: All C++ types are under the flat `LongXi` namespace (no nested namespaces).
5. **Window Title**: Changed from "Long Xi" to "LongXi" (single word).
6. **Console Allocation**: Uses `LX_DEBUG` premake define (not `_DEBUG`) to control AllocConsole in Debug builds.

## Boundary Note

This specification intentionally establishes only the native shell/bootstrap boundary. DirectX 11 render bootstrap, input systems, resource loading, and all other subsystems will be addressed by separate specifications. This spec is deliberately narrow to ensure a solid, reviewable foundation before adding complexity.

## Dependencies

### Constitutional Dependencies

- Constitution v1.0.0 (governs all technical and architectural decisions)
- Article III: Non-Negotiable Technical Baseline
- Article IV: Architectural Laws
- Article XII: Phase 1 Constitutional Guardrails

### Project Dependencies

- Premake5 build system (already configured)
- Visual Studio 2026 solution structure
- Spdlog logging library (vendor dependency)
- Existing module structure: LXCore, LXEngine, LXShell

### External Dependencies

- Windows SDK (Win32 API)
- Visual C++ Runtime (MSVC v145)
