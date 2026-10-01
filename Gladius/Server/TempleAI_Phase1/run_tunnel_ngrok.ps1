param(
    [int]$Port = 8000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Get-Command ngrok -ErrorAction SilentlyContinue)) {
    throw "ngrok was not found. Install ngrok first, then run this script again."
}

Write-Host "Starting ngrok tunnel for localhost:$Port"
Write-Host "Copy the generated https URL and add /temple/respond in Unity."

ngrok http $Port
