# SAMPR — one-command native desktop launch (no browser)
$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$exe  = Join-Path $root "build\SAMPR_artefacts\Release\SAMPR.exe"
$assistantDir = Join-Path $root "assistant"
$assistantPort = 3921

function Find-CMake {
    $candidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Find-MSBuild {
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Test-AssistantHealth {
    try {
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:$assistantPort/health" -UseBasicParsing -TimeoutSec 2
        return $resp.StatusCode -eq 200
    } catch {
        return $false
    }
}

function Get-AssistantHealthInfo {
    try {
        return Invoke-RestMethod -Uri "http://127.0.0.1:$assistantPort/health" -TimeoutSec 3
    } catch {
        return $null
    }
}

function Get-AvailableGemmaModel {
    if (-not (Get-Command ollama -ErrorAction SilentlyContinue)) { return $null }
    $list = & ollama list 2>$null
    if ($null -eq $list) { return $null }

    $priority = @('gemma2:9b', 'gemma:7b', 'gemma2:2b', 'gemma3:4b', 'gemma2', 'gemma3:12b')
    foreach ($candidate in $priority) {
        if ($list -match [regex]::Escape($candidate)) { return $candidate }
    }

    foreach ($line in $list -split "`n" | Select-Object -Skip 1) {
        $name = ($line.Trim() -split '\s+')[0]
        if ($name -like 'gemma*') { return $name }
    }
    return $null
}

function Configure-AssistantEnv {
    $envFile = Join-Path $assistantDir ".env"
    $example = Join-Path $assistantDir ".env.example"

    if (-not (Test-Path $envFile)) {
        Copy-Item $example $envFile -ErrorAction SilentlyContinue
    }

    $model = Get-AvailableGemmaModel
    if ($null -eq $model) { return }

    $content = Get-Content $envFile -ErrorAction SilentlyContinue
    if ($null -eq $content) { $content = @() }

    $updated = $false
    $newContent = foreach ($line in $content) {
        if ($line -match '^OLLAMA_MODEL=') {
            $updated = $true
            "OLLAMA_MODEL=$model"
        } else {
            $line
        }
    }

    if (-not $updated) { $newContent += "OLLAMA_MODEL=$model" }
    Set-Content -Path $envFile -Value $newContent -Encoding UTF8
}

function Start-AssistantSidecar {
    if (Test-AssistantHealth) {
        $info = Get-AssistantHealthInfo
        $model = if ($info) { $info.model } else { "unknown" }
        Write-Host "Gemma assistant already running on port $assistantPort ($model)"
        return
    }

    if (-not (Test-Path (Join-Path $assistantDir "server.js"))) {
        Write-Warning "Assistant sidecar not found - SAMPR will run without Gemma."
        return
    }

    $node = Get-Command node -ErrorAction SilentlyContinue
    if ($null -eq $node) {
        Write-Warning "Node.js not found - install Node 18+ for Gemma assistant."
        return
    }

    $gemmaModel = Get-AvailableGemmaModel
    if ($null -eq $gemmaModel) {
        Write-Warning "No Gemma model in Ollama. Recommended: ollama pull gemma2:9b"
    }

    if (-not (Test-Path (Join-Path $assistantDir "node_modules"))) {
        Write-Host "Installing assistant dependencies..."
        Push-Location $assistantDir
        & npm install --no-fund --no-audit | Out-Host
        Pop-Location
    }

    Configure-AssistantEnv

    Write-Host "Starting Gemma assistant sidecar..."
    Start-Process -FilePath "node" -ArgumentList "server.js" -WorkingDirectory $assistantDir -WindowStyle Hidden | Out-Null

    $deadline = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $deadline) {
        if (Test-AssistantHealth) {
            $info = Get-AssistantHealthInfo
            $model = if ($info) { $info.model } else { $gemmaModel }
            Write-Host "Gemma assistant online ($model)"
            return
        }
        Start-Sleep -Milliseconds 400
    }

    Write-Warning "Assistant did not respond on port $assistantPort - SAMPR will show offline status."
}

$needsBuild = -not (Test-Path $exe)

if (-not $needsBuild) {
    $srcNewer = Get-ChildItem -Path (Join-Path $root "Source") -Recurse -Include *.cpp,*.h -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -gt (Get-Item $exe).LastWriteTime }
    if ($srcNewer) { $needsBuild = $true }
}

if ($needsBuild) {
    Write-Host "Building SAMPR..."
    $cmake  = Find-CMake
    $msbuild = Find-MSBuild
    if ($null -eq $cmake -or $null -eq $msbuild) {
        throw "CMake/MSBuild not found. Install Visual Studio Build Tools with C++ workload."
    }
    & $cmake -S $root -B (Join-Path $root "build") | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    & $msbuild (Join-Path $root "build\SAMPR.sln") /p:Configuration=Release /t:SAMPR /m /v:minimal | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

if (-not (Test-Path $exe)) { throw "SAMPR.exe not found at $exe" }

Start-AssistantSidecar

Write-Host "Launching SAMPR..."
$proc = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
if ($proc) {
    Start-Sleep -Milliseconds 300
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SamprWin32 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
"@
    [SamprWin32]::ShowWindow($proc.MainWindowHandle, 9) | Out-Null
    [SamprWin32]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
}