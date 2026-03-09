[CmdletBinding()]
param(
    [string]$ExpectedYear = "2026",
    [string]$RequiredToolset = "v145",
    [string]$RequiredMsbuildMajor = "18"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

$msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if (-not $msbuildCommand) {
    Fail "msbuild.exe was not found on PATH. Open a Visual Studio 2026 Developer Command Prompt/PowerShell and try again."
}

$msbuildPath = $msbuildCommand.Source

# VS2026 ships MSBuild major version 18.x.
$msbuildVersionRaw = & $msbuildPath -version -nologo 2>$null | Select-Object -First 1
if (-not $msbuildVersionRaw -or $msbuildVersionRaw -notmatch "^$RequiredMsbuildMajor\.") {
    $actual = if ($msbuildVersionRaw) { $msbuildVersionRaw } else { "<unknown>" }
    Fail "msbuild.exe resolves to '$msbuildPath' (version $actual). Expected Visual Studio $ExpectedYear MSBuild major version $RequiredMsbuildMajor.x."
}

$vsInstallPath = $null
if ($msbuildPath -match "^(?<root>.+)\\MSBuild\\Current\\Bin(?:\\amd64)?\\MSBuild\.exe$") {
    $vsInstallPath = $Matches["root"]
} else {
    $msbuildBinPath = Split-Path -Parent $msbuildPath
    $vsInstallPath = [System.IO.Path]::GetFullPath((Join-Path $msbuildBinPath "..\..\.."))
}

if (-not $vsInstallPath -or -not (Test-Path -LiteralPath $vsInstallPath -PathType Container)) {
    Fail "Could not resolve the Visual Studio installation root from '$msbuildPath'."
}

$vcMsbuildRoot = Join-Path $vsInstallPath "MSBuild\Microsoft\VC"
if (-not (Test-Path -LiteralPath $vcMsbuildRoot -PathType Container)) {
    Fail "Missing '$vcMsbuildRoot'. Install Visual Studio $ExpectedYear with the 'Desktop development with C++' workload."
}

$toolsetProps = Get-ChildItem -Path $vcMsbuildRoot -Recurse -Filter "Toolset.props" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*\PlatformToolsets\$RequiredToolset\Toolset.props" } |
    Select-Object -First 1

if (-not $toolsetProps) {
    Fail "MSVC toolset '$RequiredToolset' is not installed. Use Visual Studio Installer to add '$RequiredToolset' under C++ build tools."
}

Write-Host "Toolchain check passed: Visual Studio $ExpectedYear MSBuild + $RequiredToolset detected." -ForegroundColor Green
