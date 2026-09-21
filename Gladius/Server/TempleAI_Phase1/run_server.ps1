param(
    [string]$HostAddress = "127.0.0.1",
    [int]$Port = 8765,
    [switch]$Reload
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

function Import-DotEnv {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        Write-Warning ".env file was not found. Copy .env.example to .env and fill GEMINI_API_KEY."
        return
    }

    Get-Content $Path | ForEach-Object {
        $line = $_.Trim()

        if ($line.Length -eq 0 -or $line.StartsWith("#")) {
            return
        }

        $parts = $line -split "=", 2
        if ($parts.Length -ne 2) {
            return
        }

        $name = $parts[0].Trim()
        $value = $parts[1].Trim()

        if (
            ($value.StartsWith('"') -and $value.EndsWith('"')) -or
            ($value.StartsWith("'") -and $value.EndsWith("'"))
        ) {
            $value = $value.Substring(1, $value.Length - 2)
        }

        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
}

Import-DotEnv -Path (Join-Path $PSScriptRoot ".env")

if (-not (Test-Path ".\.venv\Scripts\python.exe")) {
    py -m venv .venv
}

.\.venv\Scripts\python.exe -m pip install -r requirements.txt

if ([string]::IsNullOrWhiteSpace($env:GEMINI_API_KEY)) {
    throw "GEMINI_API_KEY is missing. Create .env beside run_server.ps1 and add GEMINI_API_KEY=your_key."
}

$uvicornArguments = @(
    "-m",
    "uvicorn",
    "app.main:app",
    "--host",
    $HostAddress,
    "--port",
    $Port
)

if ($Reload) {
    $uvicornArguments += "--reload"
}

& ".\.venv\Scripts\python.exe" @uvicornArguments
