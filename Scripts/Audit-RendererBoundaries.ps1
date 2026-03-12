param(
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

$targetRoot = "LongXi/LXEngine/Src"
$mapTargetRoot = "LongXi/LXGameMap/Src/Map"
$allowedDirectXRegex = @(
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]DX11Renderer\.h$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]DX11Renderer\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]SpritePipelineBridge\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]RendererFactory\.cpp$",
    "LongXi[\\/]LXEngine[\\/]Src[\\/]Renderer[\\/]Backend[\\/]DX11[\\/].+"
)

$directXPattern = '#\s*include\s*<d3d11\.h>|#\s*include\s*<dxgi\.h>|\bID3D11|\bIDXGI|\bD3D11_|\bDXGI_'
$backendIncludePattern = '#\s*include\s*["<]Renderer[\\/]DX11Renderer\.h[">]'
$backendDx11ModuleIncludePattern = '#\s*include\s*["<]Renderer[\\/]Backend[\\/]DX11[\\/][^">]+[">]'

$rawMatches = @()
try {
    $rawMatches = & rg --no-heading --line-number --color never $directXPattern $targetRoot
} catch {
    $rawMatches = @()
}

try {
    $rawMatches += (& rg --no-heading --line-number --color never $directXPattern $mapTargetRoot)
} catch {
}

try {
    $rawMatches += (& rg --no-heading --line-number --color never $backendIncludePattern $targetRoot)
} catch {
}

try {
    $rawMatches += (& rg --no-heading --line-number --color never $backendDx11ModuleIncludePattern $targetRoot)
} catch {
}

$violations = New-Object System.Collections.Generic.List[string]
foreach ($line in $rawMatches) {
    if ($line -match '^([^:]+):') {
        $file = $matches[1]
        $isAllowed = $false
        foreach ($regex in $allowedDirectXRegex) {
            if ($file -match $regex) {
                $isAllowed = $true
                break
            }
        }

        if (-not $isAllowed) {
            $violations.Add($line)
        }
    }
}

if ($Json) {
    $result = [PSCustomObject]@{
        scanned = $targetRoot
        violations = @($violations)
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
