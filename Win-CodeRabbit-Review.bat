@echo off
setlocal

rem Run CodeRabbit from WSL using repo-local wrapper script.
rem Default: plain committed review against master.

set "MODE=%~1"
if "%MODE%"=="" set "MODE=plain"

set "BASE=%~2"
if "%BASE%"=="" set "BASE=master"

for %%I in ("%~dp0.") do set "REPO_WIN=%%~fI"
for /f "usebackq delims=" %%I in (`wsl wslpath -a "%REPO_WIN%"`) do set "REPO_WSL=%%I"

if not defined REPO_WSL (
    echo [ERROR] Failed to resolve WSL path for repository.
    exit /b 1
)

echo [INFO] Running CodeRabbit review...
echo [INFO] Mode: %MODE%
echo [INFO] Base: %BASE%

wsl -e bash -lc "cd \"%REPO_WSL%\" && source ~/.bashrc >/dev/null 2>&1 && bash Scripts/Run-CodeRabbit.sh %MODE% %BASE%"
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo [ERROR] CodeRabbit review failed with exit code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

echo [SUCCESS] CodeRabbit review finished.
exit /b 0
