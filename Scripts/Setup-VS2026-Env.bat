@echo off

set "PF64=%ProgramW6432%"
if not defined PF64 set "PF64=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"
set "VSDEVCMD="

for %%R in ("%PF64%\Microsoft Visual Studio\18" "%PF86%\Microsoft Visual Studio\18") do (
    if exist "%%~R" (
        for %%E in (Community Professional Enterprise BuildTools Preview) do (
            if not defined VSDEVCMD if exist "%%~R\%%E\Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%%~R\%%E\Common7\Tools\VsDevCmd.bat"
        )
    )
)

if not defined VSDEVCMD (
    echo ERROR: Visual Studio 2026 developer environment was not found.
    echo Install VS2026 with Desktop development with C++ and MSVC v145, then retry.
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 (
    echo ERROR: Failed to initialize Visual Studio 2026 developer environment.
    exit /b 1
)

exit /b 0
