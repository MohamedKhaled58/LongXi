@echo off
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "CLANG_FORMAT="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -version [18.0^,19.0^) -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if not defined CLANG_FORMAT if exist "%%~I\VC\Tools\Llvm\x64\bin\clang-format.exe" set "CLANG_FORMAT=%%~I\VC\Tools\Llvm\x64\bin\clang-format.exe"
    )
)

if not defined CLANG_FORMAT (
    for /f "delims=" %%I in ('where clang-format.exe 2^>nul') do (
        if not defined CLANG_FORMAT set "CLANG_FORMAT=%%~fI"
    )
)

if not defined CLANG_FORMAT (
    echo ERROR: clang-format.exe was not found in Visual Studio 2026 or PATH.
    echo Install LLVM/Clang tools in Visual Studio Installer and retry.
    pause
    exit /b 1
)

set "FILE_COUNT=0"
for /f %%N in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "$files = Get-ChildItem -Recurse -File LongXi -Include *.h,*.hpp,*.cpp,*.cc,*.cxx; foreach ($f in $files) { & $env:CLANG_FORMAT -i --style=file $f.FullName }; $files.Count"') do set "FILE_COUNT=%%N"

echo Formatted %FILE_COUNT% C++ files using "%CLANG_FORMAT%".
pause
