@echo off
REM ============================================================================
REM Format-Code.bat
REM ============================================================================
REM Formats all C++ files in LongXi using clang-format.
REM
REM Usage:
REM   Scripts\Format-Code.bat           : Format all C++ files
REM   Scripts\Format-Code.bat --check   : Check mode only
REM ============================================================================

setlocal EnableDelayedExpansion

set "MODE=format"
if /I "%~1"=="--check" set "MODE=check"

set "PROJECT_ROOT=%~dp0.."
set "TARGET_DIR=%PROJECT_ROOT%\LongXi"

set "CLANG_FORMAT=clang-format"
where "%CLANG_FORMAT%" >nul 2>&1
if errorlevel 1 (
  set "CLANG_FORMAT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
)
if not exist "%CLANG_FORMAT%" (
  set "CLANG_FORMAT=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang-format.exe"
)
if not exist "%CLANG_FORMAT%" (
  echo ERROR: clang-format was not found.
  exit /b 1
)

set /a COUNT=0
for /r "%TARGET_DIR%" %%F in (*.h *.hpp *.cpp *.cc *.cxx) do (
  if /I "%MODE%"=="check" (
    "%CLANG_FORMAT%" --dry-run --Werror "%%F" >nul 2>&1
  ) else (
    "%CLANG_FORMAT%" -i "%%F"
  )
  set /a COUNT+=1
)

echo Processed !COUNT! C++ files using "%CLANG_FORMAT%".
exit /b 0