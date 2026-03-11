param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$tablesHeader = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.h"
$tablesSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11ResourceTables.cpp"
$rendererSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp"
$lifetimeContract = Join-Path $repoRoot "specs/015-render-resource-system/contracts/renderer-resource-lifetime-contract.md"

$checks = New-Object System.Collections.Generic.List[object]

function Add-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $checks.Add([PSCustomObject]@{
            name = $Name
            ok = $Passed
            detail = $Detail
        })
}

$tablesHeaderText = Get-Content -Raw $tablesHeader
$tablesSourceText = Get-Content -Raw $tablesSource
$rendererSourceText = Get-Content -Raw $rendererSource
$contractText = Get-Content -Raw $lifetimeContract

Add-Check "Resource table tracks generation" ($tablesHeaderText -match "Generation" -and $tablesSourceText -match "generation") "Resource tables must use generation for stale-handle rejection"
Add-Check "Destroy increments generation" ($tablesSourceText -match "DestroyTexture" -and $tablesSourceText -match "\+\+generation") "Destroy paths should invalidate stale handles by generation increment"
Add-Check "Force release exists for shutdown" ($tablesSourceText -match "ReleaseAll\(\)" -and $tablesSourceText -match "ForceReleased") "Shutdown must force-release remaining live resources"
Add-Check "Renderer logs shutdown cleanup summary" ($rendererSourceText -match "Resource cleanup summary") "Renderer shutdown should log per-pool cleanup statistics"
Add-Check "Lifetime contract documents stale-handle behavior" ($contractText -match "stale handle" -and $contractText -match "live = 0") "Contract must include stale-handle and shutdown cleanup guarantees"

$failed = @($checks | Where-Object { -not $_.ok })

if ($Json)
{
    [PSCustomObject]@{
        ok = ($failed.Count -eq 0)
        checks = $checks
    } | ConvertTo-Json -Depth 5
}
else
{
    foreach ($check in $checks)
    {
        $status = if ($check.ok) { "PASS" } else { "FAIL" }
        Write-Host ("[{0}] {1} - {2}" -f $status, $check.name, $check.detail)
    }
}

if ($failed.Count -gt 0)
{
    exit 1
}

