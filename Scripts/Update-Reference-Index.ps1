#!/usr/bin/env pwsh

param(
    [string]$Root = "D:/Yamen Development/Old-Reference",
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    param([string]$StartDir)
    $current = Resolve-Path $StartDir
    while ($true) {
        if (Test-Path (Join-Path $current ".specify")) {
            return $current
        }
        $parent = Split-Path $current -Parent
        if ($parent -eq $current) {
            return $null
        }
        $current = $parent
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Resolve-RepoRoot -StartDir $scriptDir
if (-not $repoRoot) {
    Write-Warning "Repository root not found. Run from inside the LongXi repo."
    exit 0
}

if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot ".specify/memory/reference-index.md"
}

if (-not (Test-Path $Root)) {
    Write-Warning "Reference root not found: $Root"
    exit 0
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Reference Projects Index")
$lines.Add("")
$lines.Add(("Generated: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")))
$lines.Add(("Root: {0}" -f $Root))
$lines.Add("")
$lines.Add("## Projects")

$projects = Get-ChildItem -Path $Root -Directory | Sort-Object Name
foreach ($project in $projects) {
    $lines.Add(("- {0} ({1})" -f $project.Name, $project.FullName))
}

$conquerRoot = Join-Path $Root "cqClient/Conquer"
if (Test-Path $conquerRoot) {
    $lines.Add("")
    $lines.Add("## cqClient/Conquer Key Directories")
    $conquerDirs = Get-ChildItem -Path $conquerRoot -Directory | Sort-Object Name
    foreach ($dir in $conquerDirs) {
        $lines.Add(("- {0} ({1})" -f $dir.Name, $dir.FullName))
    }
}

$outDir = Split-Path $OutputPath -Parent
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

Set-Content -Path $OutputPath -Value $lines -Encoding UTF8
Write-Host ("Updated reference index: {0}" -f $OutputPath)
