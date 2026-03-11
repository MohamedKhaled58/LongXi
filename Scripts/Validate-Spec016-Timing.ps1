param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$timingHeader = Join-Path $repoRoot "LongXi/LXEngine/Src/Timing/TimingService.h"
$timingSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Timing/TimingService.cpp"
$engineHeader = Join-Path $repoRoot "LongXi/LXEngine/Src/Engine/Engine.h"
$engineSource = Join-Path $repoRoot "LongXi/LXEngine/Src/Engine/Engine.cpp"
$debugUiSource = Join-Path $repoRoot "LongXi/LXShell/Src/DebugUI/DebugUI.cpp"
$boundaryAuditScript = Join-Path $repoRoot "Scripts/Audit-TimingProfilingBoundaries.ps1"

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

$timingHeaderText = Get-Content -Raw $timingHeader
$timingSourceText = Get-Content -Raw $timingSource
$engineHeaderText = Get-Content -Raw $engineHeader
$engineSourceText = Get-Content -Raw $engineSource
$debugUiSourceText = Get-Content -Raw $debugUiSource

Add-Check "Timing snapshot fields declared" ($timingHeaderText -match "FrameIndex" -and $timingHeaderText -match "DeltaTimeSeconds" -and $timingHeaderText -match "FrameTimeSeconds" -and $timingHeaderText -match "TotalTimeSeconds") "TimingSnapshot must expose all Spec 016 fields"
Add-Check "Timing service frame lifecycle implemented" ($timingSourceText -match "BeginFrame\(\)" -and $timingSourceText -match "EndFrame\(\)") "Timing service must own deterministic per-frame updates"
Add-Check "Engine exposes timing snapshot accessor" ($engineHeaderText -match "GetTimingSnapshot\(\) const") "Engine API must expose central timing snapshot"
Add-Check "Engine routes frame lifecycle through timing service" ($engineSourceText -match "m_TimingService.BeginFrame\(\)" -and $engineSourceText -match "m_TimingService.EndFrame\(\)") "Engine must call timing begin/end exactly once per frame lifecycle"
Add-Check "Scene update consumes timing-service delta" ($engineSourceText -match "m_Scene->Update\(deltaTime\)") "Engine scene update must read central delta value"
Add-Check "DebugUI consumes engine timing snapshot" ($debugUiSourceText -match "engine.GetTimingSnapshot\(\)") "DebugUI must render timing values from engine timing contract"

if (-not (Test-Path $boundaryAuditScript))
{
    Add-Check "Timing/profiling boundary audit" $false "Audit script missing: $boundaryAuditScript"
}
else
{
    try
    {
        $auditJson = & $boundaryAuditScript -Json
        if (-not $auditJson)
        {
            Add-Check "Timing/profiling boundary audit" $false "Audit script returned empty output"
        }
        else
        {
            try
            {
                $auditOutput = $auditJson | ConvertFrom-Json
                if ($null -eq $auditOutput.PSObject.Properties["ok"])
                {
                    Add-Check "Timing/profiling boundary audit" $false "Audit output missing required 'ok' field"
                }
                else
                {
                    Add-Check "Timing/profiling boundary audit" ($auditOutput.ok -eq $true) "No timing boundary violations should exist"
                }
            }
            catch
            {
                Add-Check "Timing/profiling boundary audit" $false ("Audit output is not valid JSON: {0}" -f $_.Exception.Message)
            }
        }
    }
    catch
    {
        Add-Check "Timing/profiling boundary audit" $false ("Audit invocation failed: {0}" -f $_.Exception.Message)
    }
}

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
