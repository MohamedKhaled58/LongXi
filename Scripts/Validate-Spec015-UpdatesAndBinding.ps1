param(
    [switch]$Json
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$rendererApi = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Renderer.h"
$rendererImpl = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp"
$bufferBackend = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11Buffers.cpp"
$spritePipeline = Join-Path $repoRoot "LongXi/LXEngine/Src/Renderer/Backend/DX11/DX11SpritePipeline.cpp"

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

$rendererApiText = Get-Content -Raw $rendererApi
$rendererImplText = Get-Content -Raw $rendererImpl
$bufferBackendText = Get-Content -Raw $bufferBackend
$spritePipelineText = Get-Content -Raw $spritePipeline

Add-Check "Renderer API exposes update/map/unmap" ($rendererApiText -match "UpdateBuffer\(const RendererBufferUpdateRequest& request\)" -and $rendererApiText -match "MapBuffer\(const RendererMapRequest& request, RendererMappedResource& mapped\)" -and $rendererApiText -match "UnmapBuffer\(RendererBufferHandle handle\)") "Renderer API must provide explicit update workflow"
Add-Check "Renderer API exposes explicit binding" ($rendererApiText -match "BindVertexBuffer" -and $rendererApiText -match "BindIndexBuffer" -and $rendererApiText -match "BindConstantBuffer" -and $rendererApiText -match "BindTexture") "Renderer API must provide explicit binding operations"
Add-Check "DX11 backend validates update policy" ($bufferBackendText -match "RendererResourceUsage::Static" -and $bufferBackendText -match "RendererResultCode::InvalidOperation") "Static buffers should reject mutable updates"
Add-Check "Sprite pipeline uses renderer update API" ($spritePipelineText -match "renderer.UpdateBuffer\(updateRequest\)") "Sprite upload must route through renderer UpdateBuffer"
Add-Check "Sprite pipeline uses renderer binding API" ($spritePipelineText -match "renderer.BindVertexBuffer" -and $spritePipelineText -match "renderer.BindIndexBuffer" -and $spritePipelineText -match "renderer.BindTexture") "Sprite path should bind resources via renderer API"
Add-Check "DX11 renderer logs bind/update failures" ($rendererImplText -match "BindTexture failed" -and $rendererImplText -match "UpdateBuffer failed") "DX11 renderer should emit diagnostics for policy and binding failures"

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
