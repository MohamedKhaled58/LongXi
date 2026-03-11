param(
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

$targetRoot = "LongXi/LXEngine/Src"
$allowRegex = "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]"
$pattern = "#\s*include\s*<d3d11\.h>|#\s*include\s*<dxgi\.h>|\bID3D11|\bIDXGI"

$rawMatches = @()
try {
    $rawMatches = & rg --no-heading --line-number --color never $pattern $targetRoot
} catch {
    $rawMatches = @()
}

$violations = @()
foreach ($line in $rawMatches) {
    if ($line -match '^([^:]+):') {
        $file = $matches[1]
        if ($file -notmatch $allowRegex) {
            $violations += $line
        }
    }
}

if ($Json) {
    $result = [PSCustomObject]@{
        scanned = $targetRoot
        violations = $violations
        ok = ($violations.Count -eq 0)
    }
    $result | ConvertTo-Json -Compress
} else {
    if ($violations.Count -eq 0) {
        Write-Host "[Audit] PASS: No DirectX API leakage outside renderer module"
    } else {
        Write-Host "[Audit] FAIL: DirectX API leakage detected outside renderer module"
        $violations | ForEach-Object { Write-Host "  $_" }
    }
}

if ($violations.Count -gt 0) {
    exit 1
}
