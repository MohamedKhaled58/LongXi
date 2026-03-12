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

$mapLoaderPath = "LongXi/LXGameMap/Src/Map/MapLoader.cpp"
$mapSystemPath = "LongXi/LXGameMap/Src/Map/MapSystem.cpp"
$mapTypesPath = "LongXi/LXGameMap/Src/Map/MapTypes.h"

$hasVersionGuard = (rg --no-heading --line-number --color never "version != 1004" $mapLoaderPath | Measure-Object).Count -gt 0
Add-Check "dmap version 1004 gate" $hasVersionGuard "MapLoader enforces dmap version 1004"

$hasPuzzleSupport = (rg --no-heading --line-number --color never "PUZZLE|PUZZLE2|65535" $mapLoaderPath | Measure-Object).Count -ge 2
Add-Check "puzzle signatures and sentinel" $hasPuzzleSupport "MapLoader recognizes puzzle signatures and transparent sentinel"

$hasLoadStateTransitions = (rg --no-heading --line-number --color never "MapSystemState::Loading|MapSystemState::Ready|MapSystemState::Failed" $mapSystemPath | Measure-Object).Count -ge 3
Add-Check "map load state transitions" $hasLoadStateTransitions "MapSystem transitions through Loading/Ready/Failed"

$hasTileRecordFields = (rg --no-heading --line-number --color never "TileX|TileY|Height|TextureId|Flags|ObjectRefs" $mapTypesPath | Measure-Object).Count -ge 6
Add-Check "tile record core fields" $hasTileRecordFields "TileRecord has mandatory map fields"

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
        Write-Host "[Validate-Spec017-MapLoad] PASS"
    }
    else
    {
        Write-Host "[Validate-Spec017-MapLoad] FAIL"
        $checks | Where-Object { -not $_.ok } | ForEach-Object {
            Write-Host "  - $($_.name): $($_.details)"
        }
    }
}

if (-not $ok)
{
    exit 1
}

