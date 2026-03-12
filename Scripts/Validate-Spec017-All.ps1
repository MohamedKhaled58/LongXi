param(
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

$scripts = @(
    "Scripts/Validate-Spec017-MapLoad.ps1",
    "Scripts/Validate-Spec017-TerrainRender.ps1",
    "Scripts/Validate-Spec017-MapObjects.ps1",
    "Scripts/Validate-Spec017-RendererBoundaries.ps1"
)

$results = New-Object System.Collections.Generic.List[object]
$allOk = $true

foreach ($script in $scripts)
{
    if (-not (Test-Path $script))
    {
        $allOk = $false
        $results.Add([PSCustomObject]@{
                script = $script
                ok = $false
                details = "Missing validation script"
            }) | Out-Null
        continue
    }

    try
    {
        $jsonText = & pwsh -NoProfile -ExecutionPolicy Bypass -File $script -Json
        $result = $jsonText | ConvertFrom-Json
        $scriptOk = [bool]$result.ok
        if (-not $scriptOk)
        {
            $allOk = $false
        }

        $results.Add([PSCustomObject]@{
                script = $script
                ok = $scriptOk
                details = "Executed"
            }) | Out-Null
    }
    catch
    {
        $allOk = $false
        $results.Add([PSCustomObject]@{
                script = $script
                ok = $false
                details = "Execution failed: $($_.Exception.Message)"
            }) | Out-Null
    }
}

if ($Json)
{
    [PSCustomObject]@{
        ok = $allOk
        checks = $results
    } | ConvertTo-Json -Depth 5 -Compress
}
else
{
    if ($allOk)
    {
        Write-Host "[Validate-Spec017-All] PASS"
    }
    else
    {
        Write-Host "[Validate-Spec017-All] FAIL"
        $results | Where-Object { -not $_.ok } | ForEach-Object {
            Write-Host "  - $($_.script): $($_.details)"
        }
    }
}

if (-not $allOk)
{
    exit 1
}