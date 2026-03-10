# Quickstart: DX11 Render Bootstrap

**Date**: 2026-03-10
**Spec**: [spec.md](spec.md)

## Prerequisites

- Windows 10/11 x64
- Visual Studio 2026 (MSVC v145, C++23)
- Windows SDK (includes DirectX 11 headers and libraries)
- DirectX 11–capable GPU
- Spec 001 shell bootstrap complete and building

## Build

1. Generate project files:

   ```batch
   .\Win-Generate Project.bat
   ```

   Or directly:

   ```batch
   .\Vendor\Bin\premake5.exe vs2026
   ```

2. Open `LongXi.slnx` in Visual Studio 2026

3. Build Debug (x64):
   - Build → Build Solution (F7)
   - Verify 3 projects build: `LXCore.lib`, `LXEngine.lib`, `LXShell.exe`

## Run

1. Launch `Build/Debug/Executables/LXShell.exe`

2. Expected behavior:
   - Debug console window appears
   - Log messages show renderer initialization:

     ```text
     [LXEngine] Application initializing...
     [LXEngine] DX11 renderer initialized (Feature Level: 11_0)
     [LXEngine] Main window created and shown (1024x768)
     [LXEngine] Application initialized successfully
     ```

   - Application window displays **cornflower blue** background (not black)
   - Window is responsive and resizable

3. Close the window:
   - Expected clean shutdown logs:

     ```text
     [LXEngine] WM_CLOSE received — destroying window
     [LXEngine] Application shutting down...
     [LXEngine] DX11 renderer shutdown complete
     ```

## Verify

### Build Verification

| Configuration | Expected Result                               |
| ------------- | --------------------------------------------- |
| Debug         | Builds with 0 errors, DX11 debug layer active |
| Release       | Builds with 0 errors, no debug layer          |
| Dist          | Builds with 0 errors, optimized               |

### Visual Verification

| Test               | Pass Criteria                                  |
| ------------------ | ---------------------------------------------- |
| Launch             | Window shows cornflower blue (not black)       |
| Resize             | Blue color persists at new size, no corruption |
| Minimize + Restore | Blue color returns correctly                   |
| Close              | Clean exit, no crash, no debug layer warnings  |

## Troubleshooting

**Black window (no DX11 rendering)**:

- Verify `d3d11.lib` and `dxgi.lib` are linked (check LXEngine premake5.lua)
- Check debug console for error messages
- Verify GPU supports DirectX 11

**Debug layer warnings**:

- Check for leaked COM objects at shutdown
- Verify render target is released before ResizeBuffers
- Verify all COM objects released before device

**Crash on resize**:

- Ensure render target view released before ResizeBuffers call
- Check for zero-area resize (minimized window)
