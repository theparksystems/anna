# SAMPR — one-command native desktop launch (no browser)
$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$exe  = Join-Path $root "build\SAMPR_artefacts\Release\SAMPR.exe"

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