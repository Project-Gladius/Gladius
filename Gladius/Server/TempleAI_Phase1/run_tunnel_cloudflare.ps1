param(
    [string]$LocalUrl = "http://localhost:8000"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Get-Command cloudflared -ErrorAction SilentlyContinue)) {
    throw "cloudflared was not found. Install Cloudflare Tunnel first, then run this script again."
}

Write-Host "Starting Cloudflare quick tunnel for $LocalUrl"
Write-Host "Copy the generated https://*.trycloudflare.com URL and add /temple/respond in Unity."

cloudflared tunnel --url $LocalUrl
