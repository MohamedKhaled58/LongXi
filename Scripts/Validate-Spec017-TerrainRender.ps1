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

$mapCameraPath = "LongXi/LXGameMap/Src/Map/MapCamera.cpp"
$tileGridPath = "LongXi/LXGameMap/Src/Map/TileGrid.cpp"
$mapSystemPath = "LongXi/LXGameMap/Src/Map/MapSystem.cpp"
$tileRendererPath = "LongXi/LXGameMap/Src/Map/TileRenderer.cpp"

$hasIsoTransform = (rg --no-heading --line-number --color never "tileX - tileY|tileX \+ tileY" $mapCameraPath | Measure-Object).Count -ge 2
Add-Check "isometric transform formula" $hasIsoTransform "MapCamera applies Tile->World isometric conversion"

$hasVisibleWindow = (rg --no-heading --line-number --color never "ComputeVisibleWindow|m_CullMarginTilesX|m_CullMarginTilesY" $mapCameraPath | Measure-Object).Count -ge 3
Add-Check "visible window with margins" $hasVisibleWindow "MapCamera computes bounded visible tile window"

$hasDiagonalTraversal = (rg --no-heading --line-number --color never "minDepth|maxDepth|depth =" $tileGridPath | Measure-Object).Count -ge 3
Add-Check "diagonal traversal" $hasDiagonalTraversal "TileGrid enumerates tiles by depth-key sweep"

$hasRendererSubmission = (rg --no-heading --line-number --color never "BindVertexBuffer|BindIndexBuffer|DrawIndexed" $tileRendererPath | Measure-Object).Count -ge 3
Add-Check "renderer API draw submission" $hasRendererSubmission "TileRenderer submits draw calls through renderer API"

$hasRenderStats = (rg --no-heading --line-number --color never "VisibleTiles|DrawCalls|VisibleObjects|AnimatedTiles" $mapSystemPath | Measure-Object).Count -ge 4
Add-Check "map render stats" $hasRenderStats "MapSystem publishes map frame stats"

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
        Write-Host "[Validate-Spec017-TerrainRender] PASS"
    }
    else
    {
        Write-Host "[Validate-Spec017-TerrainRender] FAIL"
        $checks | Where-Object { -not $_.ok } | ForEach-Object {
            Write-Host "  - $($_.name): $($_.details)"
        }
    }
}

if (-not $ok)
{
    exit 1
}
