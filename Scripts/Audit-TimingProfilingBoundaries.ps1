param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$targetRoot = "LongXi/LXEngine/Src"

$allowedChronoRegex = @(
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Engine[\\/]Engine\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Timing[\\/]TimingService\.h$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Timing[\\/]TimingService\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Profiling[\\/]ProfileScope\.h$"
)

$allowedTimingHeaderIncludeRegex = @(
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Engine[\\/]Engine\.h$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Engine[\\/]Engine\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Timing[\\/]TimingService\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Timing[\\/]TimingService\.h$"
)

$chronoPattern = '#\s*include\s*<chrono>|\bstd::chrono\b'
$timingHeaderIncludePattern = '#\s*include\s*["<]Timing[\\/]TimingService\.h[">]'

$violations = New-Object System.Collections.Generic.List[string]

function CollectViolations
{
    param(
        [string]$Pattern,
        [string[]]$AllowedRegex
    )

    $rawMatches = @()
    try
    {
        $rawMatches = & rg --no-heading --line-number --color never $Pattern $targetRoot
    }
    catch
    {
        $rawMatches = @()
    }

    foreach ($line in $rawMatches)
    {
        if ($line -notmatch '^([^:]+):')
        {
            continue
        }

        $file = $matches[1]
        $isAllowed = $false
        foreach ($regex in $AllowedRegex)
        {
            if ($file -match $regex)
            {
                $isAllowed = $true
                break
            }
        }

        if (-not $isAllowed)
        {
            $violations.Add($line)
        }
    }
}

CollectViolations -Pattern $chronoPattern -AllowedRegex $allowedChronoRegex
CollectViolations -Pattern $timingHeaderIncludePattern -AllowedRegex $allowedTimingHeaderIncludeRegex

if ($Json)
{
    [PSCustomObject]@{
        scanned = $targetRoot
        violations = @($violations)
        ok = ($violations.Count -eq 0)
    } | ConvertTo-Json -Compress
}
else
{
    if ($violations.Count -eq 0)
    {
        Write-Host "[Audit] PASS: Timing/profiling boundaries are respected"
    }
    else
    {
        Write-Host "[Audit] FAIL: Timing/profiling boundary violations detected"
        $violations | ForEach-Object { Write-Host "  $_" }
    }
}

if ($violations.Count -gt 0)
{
    exit 1
}
