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

:: Long Xi solution generator — uses vs2026 action (Premake5 beta8+)
:: Validated toolchain: Visual Studio with vs2026 action.
:: One Long Xi.slnx at root; each .vcxproj in its own project folder.
Vendor\Bin\premake5.exe vs2026
pause
