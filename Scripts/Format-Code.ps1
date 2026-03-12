# ============================================================================
# Format-Code.ps1
# ============================================================================
# Formats all C++ files in LongXi using clang-format.
#
# Usage:
#   .\Scripts\Format-Code.ps1
#   .\Scripts\Format-Code.ps1 -Check
# ============================================================================

param(
    [switch]$Check
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$targetDir = Join-Path $projectRoot 'LongXi'

$clangCandidates = @(
    'clang-format',
    'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang-format.exe'
)

$clang = $null
foreach ($candidate in $clangCandidates)
{
    try
    {
        $cmd = Get-Command $candidate -ErrorAction Stop
        $clang = $cmd.Source
        break
    }
    catch
    {
    }
}

if ([string]::IsNullOrWhiteSpace($clang))
{
    Write-Error 'clang-format was not found.'
    exit 1
}

$extensions = @('*.h', '*.hpp', '*.cpp', '*.cc', '*.cxx')
$files = foreach ($ext in $extensions) { Get-ChildItem -Path $targetDir -Recurse -File -Filter $ext }

$count = 0
foreach ($file in $files)
{
    if ($Check)
    {
        & $clang --dry-run --Werror $file.FullName | Out-Null
    }
    else
    {
        & $clang -i $file.FullName | Out-Null
    }
    $count++
}

Write-Host "Processed $count C++ files using '$clang'."