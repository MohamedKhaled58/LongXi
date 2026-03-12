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

$mapObjectsPath = "LongXi/LXGameMap/Src/Map/MapObjects.cpp"
$mapSystemPath = "LongXi/LXGameMap/Src/Map/MapSystem.cpp"
$mapTypesPath = "LongXi/LXGameMap/Src/Map/MapTypes.h"

$hasObjectSort = (rg --no-heading --line-number --color never "RenderLayer|GetDepthKey|InsertionOrder" $mapObjectsPath | Measure-Object).Count -ge 3
Add-Check "object sort key" $hasObjectSort "MapObjects sorts by layer/depth/insertion order"

$hasAnimationState = (rg --no-heading --line-number --color never "MapAnimationState|FrameStepMilliseconds|CurrentFrame" $mapTypesPath | Measure-Object).Count -ge 3
Add-Check "animation state fields" $hasAnimationState "MapAnimationState stores deterministic animation fields"

$hasTimingDrivenAnimation = (rg --no-heading --line-number --color never "DeltaTimeSeconds|FrameStepMilliseconds|UpdateAnimations" $mapSystemPath | Measure-Object).Count -ge 3
Add-Check "timing-driven animation" $hasTimingDrivenAnimation "MapSystem advances animation from engine timing"

$hasObjectStats = (rg --no-heading --line-number --color never "VisibleObjects|AnimatedTiles" $mapSystemPath | Measure-Object).Count -ge 2
Add-Check "object and animation stats" $hasObjectStats "MapSystem reports object and animation counts"

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
        Write-Host "[Validate-Spec017-MapObjects] PASS"
    }
    else
    {
        Write-Host "[Validate-Spec017-MapObjects] FAIL"
        $checks | Where-Object { -not $_.ok } | ForEach-Object {
            Write-Host "  - $($_.name): $($_.details)"
        }
    }
}

if (-not $ok)
{
    exit 1
}
