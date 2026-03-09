@echo off
setlocal
cd /d "%~dp0"

set "CLANG_FORMAT="
for %%P in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
    "%ProgramFiles%\Microsoft Visual Studio\18\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe"
    "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\VC\Tools\Llvm\x64\bin\clang-format.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\18\Enterprise\VC\Tools\Llvm\x64\bin\clang-format.exe"
) do (
    if not defined CLANG_FORMAT if exist %%~P set "CLANG_FORMAT=%%~P"
)

if not defined CLANG_FORMAT (
    echo ERROR: clang-format.exe was not found in a VS2026 LLVM toolchain path.
    echo Install LLVM/Clang tools in Visual Studio Installer and retry.
    pause
    exit /b 1
)

set "FILE_COUNT=0"
for /f %%N in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "$files = Get-ChildItem -Recurse -File Modules,Executables,Tests,ArchVerify -Include *.h,*.hpp,*.cpp,*.cc,*.cxx; foreach ($f in $files) { & $env:CLANG_FORMAT -i --style=file $f.FullName }; $files.Count"') do set "FILE_COUNT=%%N"

echo Formatted %FILE_COUNT% C++ files using "%CLANG_FORMAT%".
pause
