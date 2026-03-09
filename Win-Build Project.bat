@echo off
setlocal
cd /d "%~dp0"

call ".\Scripts\Setup-VS2026-Env.bat"
if errorlevel 1 (
    echo.
    echo VS2026 environment setup failed. Build stopped.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File ".\Scripts\Require-VS2026-Toolchain.ps1"
if errorlevel 1 (
    echo.
    echo Toolchain guard failed. Build stopped.
    pause
    exit /b 1
)

if not exist "LongXi.slnx" (
    echo LongXi.slnx was not found. Run "Win-Generate Project.bat" first.
    pause
    exit /b 1
)

set "CFG=%~1"
if "%CFG%"=="" set "CFG=Debug"
set "PLAT=%~2"
if "%PLAT%"=="" set "PLAT=x64"

set "MSBUILD_EXE=%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD_EXE%" set "MSBUILD_EXE=msbuild"

"%MSBUILD_EXE%" "LongXi.slnx" /m /p:Configuration=%CFG%;Platform=%PLAT%;PlatformToolset=v145
set "BUILD_EXIT=%ERRORLEVEL%"
if not "%BUILD_EXIT%"=="0" (
    echo.
    echo Build failed with exit code %BUILD_EXIT%.
    pause
    exit /b %BUILD_EXIT%
)

echo Build succeeded (Configuration=%CFG%, Platform=%PLAT%, PlatformToolset=v145).
pause
