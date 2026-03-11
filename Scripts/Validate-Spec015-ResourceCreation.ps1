param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$rendererTypes = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/RendererTypes.h"
$rendererApi = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Renderer.h"
$textureManager = Join-Path $repoRoot "LongXi/LXEngine/Src/Texture/TextureManager.cpp"
$auditScript = Join-Path $repoRoot "Scripts/Audit-RendererBoundaries.ps1"

$checks = New-Object System.Collections.Generic.List[object]

function Add-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $checks.Add([PSCustomObject]@{
            name = $Name
            ok = $Passed
            detail = $Detail
        })
}

$rendererTypesText = Get-Content -Raw $rendererTypes
$rendererApiText = Get-Content -Raw $rendererApi
$textureManagerText = Get-Content -Raw $textureManager

Add-Check "Renderer descriptors declared" ($rendererTypesText -match "struct RendererTextureDesc" -and $rendererTypesText -match "struct RendererBufferDesc" -and $rendererTypesText -match "struct RendererShaderDesc") "RendererTypes descriptors must exist"
Add-Check "Opaque handles declared" ($rendererTypesText -match "RendererHandleId" -and $rendererTypesText -match "RendererTextureHandle") "Renderer handles must use slot/generation identity"
Add-Check "Renderer API exposes descriptor creation" ($rendererApiText -match "CreateTexture\(const RendererTextureDesc& desc\)" -and $rendererApiText -match "CreateVertexBuffer\(const RendererBufferDesc& desc\)") "Renderer API must expose descriptor-based create methods"
Add-Check "TextureManager uses descriptor path" ($textureManagerText -match "RendererTextureDesc" -and $textureManagerText -match "CreateTexture\(textureDesc\)") "TextureManager should create textures through descriptor API"

$auditOutput = & pwsh -NoProfile -ExecutionPolicy Bypass -File $auditScript -Json | ConvertFrom-Json
Add-Check "Renderer boundary audit" ($auditOutput.ok -eq $true) "DirectX leakage must remain confined to renderer backend files"

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

