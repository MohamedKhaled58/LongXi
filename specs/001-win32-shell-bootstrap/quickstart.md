# Quickstart Guide: Win32 Shell Bootstrap

**Feature**: Spec 001 — Win32 Shell Bootstrap
**Date**: 2026-03-10
**Phase**: Phase 1 Design

## Overview

This guide provides step-by-step instructions for building and running the Win32 Shell Bootstrap application. This is a development-focused quickstart for the first implementation phase of the Long Xi Modernized Clone project.

## Prerequisites

### Required Software

1. **Visual Studio 2026** with MSVC v145 toolchain
   - Desktop development with C++ workload
   - Windows SDK components
   - C++23 language support

2. **Premake5** (included in repository)
   - Located at: `Vendor/Bin/premake5.exe`
   - Used for generating Visual Studio project files

3. **Git** (optional, for version control)
   - For cloning repository and managing branches

### System Requirements

- Windows 10 or later (x64)
- 100 MB free disk space for build artifacts
- Administrator access (for first-time setup if needed)

## Building the Solution

### Step 1: Generate Project Files

```batch
cd /d "D:\Yamen Development\LongXi"
Win-Generate Project.bat
```

**Expected Output**:

```
Premake5 Beta7.2
Generating LongXi.slnx...
Generating LongXi/LXCore/LXCore.vcxproj...
Generating LongXi/LXEngine/LXEngine.vcxproj...
Generating LongXi/LXShell/LXShell.vcxproj...
Done.
```

**Troubleshooting**:

- If Premake5 fails, verify `Vendor/Bin/premake5.exe` exists
- Check that Premake5 beta8+ is installed (vs2026 action support)
- Ensure Visual Studio 2026 is properly installed

### Step 2: Open Solution

1. Navigate to the repository root
2. Double-click `LongXi.slnx` or open Visual Studio 2026
3. Select **Debug** or **Release** configuration

### Step 3: Build Solution

**Using the provided batch script:**

```batch
Win-Build Project.bat Debug x64
```

**Or build from Visual Studio:**

1. Select **Build → Build Solution** (F7)
2. Choose configuration: **Debug**, **Release**, or **Dist**
3. Wait for build to complete

**Expected Build Artifacts:**

- `Build/Debug/Core/LXCore.lib` - Core static library
- `Build/Debug/Core/LXEngine.lib` - Engine static library
- `Build/Debug/Executables/LXShell.exe` - Executable

**Build Time:**

- First build: ~30-60 seconds (full compilation)
- Incremental builds: ~5-15 seconds

**Troubleshooting Build Errors:**

| Error                                   | Solution                                           |
| --------------------------------------- | -------------------------------------------------- |
| "Cannot open include file 'spdlog/...'" | Ensure `Vendor/Spdlog/include` is in include paths |
| "unresolved external symbol"            | Check that Spdlog static library is linked         |
| "LNK2019" errors                        | Verify LXEngine links LXCore                       |
| "C++23 features not supported"          | Ensure MSVC v145 toolchain is selected             |

## Running the Application

### Development Build (Recommended for Testing)

**From Visual Studio:**

1. Set startup project to **LXShell**
2. Select **Debug** configuration
3. Press **F5** (Start Debugging)

**From Command Line:**

```batch
cd "D:\Yamen Development\LongXi"
Build\Debug\Executables\LXShell.exe
```

### Expected Behavior

#### 1. Application Launch (0-2 seconds)

**Console Output:**

```
[2026-03-10 14:30:15.123] [info] Application starting...
[2026-03-10 14:30:15.234] [info] AllocConsole succeeded
[2026-03-10 14:30:15.256] [info] Spdlog initialized
[2026-03-10 14:30:15.278] [info] Win32Window::Create() succeeded
[2026-03-10 14:30:15.290] [info] Application initialized successfully
```

**Visual Result:**

- Debug console window appears (black background, colored text)
- Main application window titled "LongXi" appears (1024x768)
- Window is in normal mode (not fullscreen, not maximized)
- Window is resizeable (WS_OVERLAPPEDWINDOW style)

#### 2. Application Runtime (Stable)

**Console Output:**

```
[2026-03-10 14:30:15.312] [info] Application::Run() - entering message pump
```

**Visual Result:**

- Window remains open and responsive
- Window can be moved, resized, minimized, maximized
- Application stays running in message pump loop
- No CPU spikes or hangs (message pump is idle when no messages)

#### 3. Application Shutdown (Close Window)

**Actions:**

- Click **X button** on window, **Alt-F4**, or taskbar close

**Console Output:**

```
[2026-03-10 14:30:45.123] [info] WM_CLOSE received
[2026-03-10 14:30:45.134] [info] WM_DESTROY received, posting quit message
[2026-03-10 14:30:45.145] [info] Application::Run() - exiting message pump
[2026-03-10 14:30:45.156] [info] Application::Shutdown() - destroying resources
[2026-03-10 14:30:45.167] [info] Win32Window::Destroy() succeeded
[2026-03-10 14:30:45.178] [info] Application shutting down...
```

**Visual Result:**

- Main window closes cleanly
- Debug console remains visible (for log review)
- Process terminates cleanly
- No zombie processes in Task Manager

## Testing Acceptance Criteria

### AC-001: Build Success

**Verification:**

```batch
cd /d "D:\Yamen Development\LongXi"
Win-Build Project.bat Debug x64
Win-Build Project.bat Release x64
Win-Build Project.bat Dist x64
```

**Expected:** All three builds complete without errors or warnings

### AC-002: Window Appearance

**Verification:**

- Launch `LXShell.exe`
- Observe window titled "LongXi"
- Measure window size (should be 1024x768)
- Confirm normal windowed mode (not fullscreen)

**Expected:** Window appears with correct title, size, and mode

### AC-003: Debug Console

**Verification:**

- Launch `LXShell.exe`
- Observe console window appears
- Check for "Application starting..." message
- Check for "Application shutting down..." message

**Expected:** Console visible with startup/shutdown messages

### AC-004: Runtime Stability

**Verification:**

- Leave application running for 5 minutes
- Move window, resize window, minimize/restore
- Observe no crashes or hangs

**Expected:** Application remains stable, no crashes, responsive

### AC-005: Clean Shutdown

**Verification:**

- Close window via X button or Alt-F4
- Observe console shutdown sequence
- Check Task Manager for zombie processes

**Expected:** Clean shutdown within 1 second, no zombie processes

### AC-007: Thin Entrypoint

**Verification:**

- Open `LongXi/LXShell/Src/main.cpp`
- Count lines of code (excluding comments/blanks)

**Expected:** Less than 20 lines in WinMain function

### AC-008: Lifecycle Ownership

**Verification:**

- Review `LXEngine/Src/Application/Application.h`
- Confirm Initialize(), Run(), Shutdown() methods exist
- Check that all lifecycle logic is in Application class

**Expected:** All lifecycle logic in Application class, clear separation

## Code Formatting

Before committing code, run the formatter:

```batch
Win-Format Code.bat
```

This formats all C++ files in the `LongXi/` directory using `.clang-format` configuration.

## Development Workflow

### Typical Development Cycle

1. **Edit source files** (Application.h, Win32Window.h, main.cpp, etc.)
2. **Run formatter** (Win-Format Code.bat)
3. **Build solution** (Win-Build Project.bat Debug x64)
4. **Run and test** (Launch LXShell.exe, verify behavior)
5. **Debug issues** (Use Visual Studio debugger if needed)
6. **Repeat** until implementation complete

### File Organization

**Key Source Files:**

- `LongXi/LXCore/Src/Core/Logging/` - Logging macros and Spdlog setup
- `LongXi/LXEngine/Src/Application/Application.h/cpp` - Application class
- `LongXi/LXEngine/Src/Window/Win32Window.h/cpp` - Window wrapper class
- `LongXi/LXShell/Src/main.cpp` - WinMain entrypoint

**Build Artifacts:**

- `Build/Debug/Core/`, `Build/Release/Core/`, `Build/Dist/Core/` - Static libraries
- `Build/Debug/Executables/`, `Build/Release/Executables/` - Executables

## Troubleshooting Common Issues

### Issue: "No debug console appears"

**Possible Causes:**

1. Build is Release/Dist configuration (console only in Debug builds)
2. AllocConsole failed silently
3. Console stream redirection not configured

**Solutions:**

1. Ensure Debug configuration
2. Check logs for "AllocConsole succeeded" message
3. Verify stdout/stderr redirection in Initialize()

### Issue: "Window doesn't appear"

**Possible Causes:**

1. Initialize() returned false (check return value in WinMain)
2. Window created but off-screen
3. Window creation failed (check console for error message)

**Solutions:**

1. Add error checking after Initialize() call
2. Verify Show() is called with proper nCmdShow
3. Check console for window creation error messages

### Issue: "Application hangs on shutdown"

**Possible Causes:**

1. Infinite loop in Run() method
2. WM_QUIT message not posted
3. Window procedure not calling DefWindowProc

**Solutions:**

1. Add debug logging before/after GetMessage loop
2. Verify WM_DESTROY handler calls PostQuitMessage
3. Check that WindowProc properly chains messages

### Issue: "Build errors about missing Windows SDK"

**Possible Causes:**

1. Windows SDK not installed
2. Include paths not configured correctly
3. Wrong Visual Studio components installed

**Solutions:**

1. Install Windows SDK via Visual Studio Installer
2. Verify Premake5 generated correct project settings
3. Ensure "Desktop development with C++" workload is installed

### Issue: "Spdlog linking errors"

**Possible Causes:**

1. Spdlog static library not found
2. Runtime library mismatch (Debug/Release)
3. Include path incorrect

**Solutions:**

1. Verify `Vendor/Spdlog/include` is in include paths
2. Check Spdlog library path in linker settings
3. Ensure runtime library compatibility (MultiThreadedDebugDLL for Debug)

## Next Steps

After successfully building and running the Win32 Shell Bootstrap:

1. **Verify all acceptance criteria** using the testing section above
2. **Review code quality** using `Win-Format Code.bat`
3. **Check constitutional compliance** via code review
4. **Proceed to next specification** (DX11 bootstrap or input systems)

## Summary

This quickstart provides:

- ✅ Clear build instructions using provided batch scripts
- ✅ Run instructions with expected behavior
- ✅ Troubleshooting guide for common issues
- ✅ Acceptance criteria verification steps
- ✅ Development workflow guidance

The Win32 Shell Bootstrap is now ready for implementation with complete build, run, and test procedures.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
