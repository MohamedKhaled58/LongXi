param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$debugUiHeader = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/DebugUI.h"
$debugUiSource = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/DebugUI.cpp"
$profilerPanelHeader = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/Panels/ProfilerPanel.h"
$profilerPanelSource = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/Panels/ProfilerPanel.cpp"
$enginePanelSource = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/Panels/EnginePanel.cpp"
$mainSource = Join-Path $repoRoot "LongXi/LXShell/Src/main.cpp"

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

$debugUiHeaderText = Get-Content -Raw $debugUiHeader
$debugUiSourceText = Get-Content -Raw $debugUiSource
$profilerPanelHeaderText = Get-Content -Raw $profilerPanelHeader
$profilerPanelSourceText = Get-Content -Raw $profilerPanelSource
$enginePanelSourceText = Get-Content -Raw $enginePanelSource
$mainSourceText = Get-Content -Raw $mainSource

Add-Check "Profiler view models declared" ($debugUiHeaderText -match "struct ProfilerEntryViewModel" -and $debugUiHeaderText -match "struct ProfilerPanelViewModel") "DebugUI should expose profiler panel data model"
Add-Check "Profiler panel declaration exists" ($profilerPanelHeaderText -match "class ProfilerPanel" -and $profilerPanelHeaderText -match "Render\(const ProfilerPanelViewModel&") "Profiler panel interface must exist"
Add-Check "DebugUI populates profiler data from engine snapshots" ($debugUiSourceText -match "engine.GetLastFrameProfileSnapshot\(\)" -and $debugUiSourceText -match "engine.GetTimingSnapshot\(\)") "DebugUI must map engine timing/profiling snapshots"
Add-Check "DebugUI renders profiler panel" ($debugUiSourceText -match "ProfilerPanel::Render\(m_ProfilerPanel\)") "Profiler panel should render every frame when visible"
Add-Check "Profiler panel renders scope table" ($profilerPanelSourceText -match "Duration \(ms\)" -and $profilerPanelSourceText -match "CallCount") "Profiler panel should display scope duration and calls"
Add-Check "Engine metrics panel shows timing summary" ($enginePanelSourceText -match "Frame Index" -and $enginePanelSourceText -match "Delta Time" -and $enginePanelSourceText -match "Profile Scopes") "Engine panel should expose timing/profiling headline metrics"
Add-Check "Main loop wires profiler panel visibility hotkey" (($mainSourceText -match "Key::F6") -and $mainSourceText -match "ToggleProfilerPanel" -and ($mainSourceText -match "IsKeyPressed\(Key::F6\)" -or $mainSourceText -match "m_F6WasDown")) "Runtime should provide profiler panel visibility path"

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
