@echo off
setlocal
cd /d "%~dp0"

call ".\Scripts\Setup-VS2026-Env.bat"
if errorlevel 1 (
    echo.
    echo VS2026 environment setup failed. Generation stopped.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File ".\Scripts\Require-VS2026-Toolchain.ps1"
if errorlevel 1 (
    echo.
    echo Toolchain guard failed. Generation stopped.
    pause
    exit /b 1
)

set "PREMAKE_EXE=%~dp0Vendor\Bin\premake5.exe"
if not exist "%PREMAKE_EXE%" (
    for /f "delims=" %%I in ('where premake5.exe 2^>nul') do (
        if not defined PREMAKE_FROM_PATH set "PREMAKE_FROM_PATH=%%~fI"
    )
    if defined PREMAKE_FROM_PATH set "PREMAKE_EXE=%PREMAKE_FROM_PATH%"
)

if not exist "%PREMAKE_EXE%" (
    echo ERROR: premake5.exe was not found.
    echo Place it at Vendor\Bin\premake5.exe or add premake5.exe to PATH.
    pause
    exit /b 1
)

:: Long Xi solution generator (Visual Studio 2026 action).
"%PREMAKE_EXE%" vs2026
if errorlevel 1 (
    echo.
    echo Premake generation failed.
    pause
    exit /b 1
)

pause
