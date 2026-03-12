param(
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

$checks = New-Object System.Collections.Generic.List[object]

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Details
    )

    $checks.Add([PSCustomObject]@{
            name = $Name
            ok = $Ok
            details = $Details
        }) | Out-Null
}

$auditScript = "Scripts/Audit-RendererBoundaries.ps1"
$mapRoot = "LongXi/LXGameMap/Src/Map"

$boundaryOk = $false
$boundaryDetails = "Audit script did not run"
if (-not (Test-Path $auditScript))
{
    Add-Check "renderer boundary audit script" $false "Missing Scripts/Audit-RendererBoundaries.ps1"
}
else
{
    try
    {
        $auditJson = & pwsh -NoProfile -ExecutionPolicy Bypass -File $auditScript -Json
        $auditOutput = $auditJson | ConvertFrom-Json
        $boundaryOk = [bool]$auditOutput.ok
        $boundaryDetails = if ($boundaryOk) { "No boundary violations reported" } else { "Boundary violations detected" }
    }
    catch
    {
        $boundaryOk = $false
        $boundaryDetails = "Boundary audit failed: $($_.Exception.Message)"
    }

    Add-Check "renderer boundary audit" $boundaryOk $boundaryDetails
}

$forbiddenDirectX = (rg --no-heading --line-number --color never "#\s*include\s*<d3d11\.h>|#\s*include\s*<dxgi\.h>|\bID3D11|\bIDXGI|\bD3D11_|\bDXGI_" $mapRoot | Measure-Object).Count -eq 0
Add-Check "map module directx leakage" $forbiddenDirectX "Map module has no DirectX headers/types"

$apiSurface = (rg --no-heading --line-number --color never "Renderer.h|RendererTypes.h" "LongXi/LXGameMap/Src/Map/MapSystem.cpp" "LongXi/LXGameMap/Src/Map/TileRenderer.cpp" | Measure-Object).Count -ge 2
Add-Check "renderer api include surface" $apiSurface "Map rendering includes renderer abstraction headers"

$ok = ($checks | Where-Object { -not $_.ok }).Count -eq 0

if ($Json)
{
    [PSCustomObject]@{
        ok = $ok
        checks = $checks
    } | ConvertTo-Json -Depth 5 -Compress
}
else
{
    if ($ok)
    {
        Write-Host "[Validate-Spec017-RendererBoundaries] PASS"
    }
    else
    {
        Write-Host "[Validate-Spec017-RendererBoundaries] FAIL"
        $checks | Where-Object { -not $_.ok } | ForEach-Object {
            Write-Host "  - $($_.name): $($_.details)"
        }
    }
}

if (-not $ok)
{
    exit 1
}
