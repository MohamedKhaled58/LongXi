param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$profileScopeHeader = Join-Path $repoRoot "LongXi/LXEngine/Src/Profiling/ProfileScope.h"
$collectorHeader = Join-Path $repoRoot "LongXi/LXEngine/Src/Profiling/ProfilerCollector.h"
$collectorSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Profiling/ProfilerCollector.cpp"
$engineSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Engine/Engine.cpp"
$sceneSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Scene/Scene.cpp"
$spriteSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/SpriteRenderer.cpp"

$checks = New-Object System.Collections.Generic.List[object]

function Add-Check
{
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $checks.Add(
        [PSCustomObject]@{
            name = $Name
            ok = $Passed
            detail = $Detail
        }
    )
}

$profileScopeText = Get-Content -Raw $profileScopeHeader
$collectorHeaderText = Get-Content -Raw $collectorHeader
$collectorSourceText = Get-Content -Raw $collectorSource
$engineSourceText = Get-Content -Raw $engineSource
$sceneSourceText = Get-Content -Raw $sceneSource
$spriteSourceText = Get-Content -Raw $spriteSource

Add-Check "Profile macro build gating exists" ($profileScopeText -match "LX_DEBUG" -and $profileScopeText -match "LX_DEV" -and $profileScopeText -match "LX_PROFILE_SCOPE") "Profiling macro must be enabled only in development builds"
Add-Check "Collector data contract includes scope/time/count" ($collectorHeaderText -match "ScopeName" -and $collectorHeaderText -match "TotalDurationMicroseconds" -and $collectorHeaderText -match "CallCount") "Frame profile entries must include required fields"
Add-Check "Collector supports per-frame begin/end lifecycle" ($collectorHeaderText -match "BeginFrame" -and $collectorHeaderText -match "EndFrame") "Collector must reset and publish per-frame snapshot"
Add-Check "Collector aggregates and sorts entries" ($collectorSourceText -match "m_ActiveEntries" -and $collectorSourceText -match "sort\(") "Collector should aggregate calls and output deterministic ordering"
Add-Check "Engine hooks profiling begin/end each frame" ($engineSourceText -match "m_ProfilerCollector.BeginFrame" -and $engineSourceText -match "m_ProfilerCollector.EndFrame") "Engine must drive profiler lifecycle through frame boundaries"
Add-Check "Engine frame scopes instrumented" ($engineSourceText -match 'LX_PROFILE_SCOPE\("Engine.Update"\)' -and $engineSourceText -match 'LX_PROFILE_SCOPE\("Engine.Render"\)') "Core engine stages should be instrumented"
Add-Check "Scene scopes instrumented" ($sceneSourceText -match 'LX_PROFILE_SCOPE\("Scene.Update"\)' -and $sceneSourceText -match 'LX_PROFILE_SCOPE\("Scene.Render"\)') "Scene update/render should be instrumented"
Add-Check "Sprite scopes instrumented" ($spriteSourceText -match 'LX_PROFILE_SCOPE\("SpriteRenderer.Begin"\)' -and $spriteSourceText -match 'LX_PROFILE_SCOPE\("SpriteRenderer.FlushBatch"\)') "Sprite renderer path should be instrumented"

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
