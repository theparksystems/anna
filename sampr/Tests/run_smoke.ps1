# SAMPR post-build smoke tests
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"

if (-not (Test-Path $cmake)) { throw "cmake not found at $cmake" }
if (-not (Test-Path $msbuild)) { throw "msbuild not found at $msbuild" }

Write-Host "== Configure =="
& $cmake -S $root -B "$root\build" | Out-Host

Write-Host "== Build Release =="
& $msbuild "$root\build\SAMPR.sln" /p:Configuration=Release /m /v:minimal | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

$app = "$root\build\SAMPR_artefacts\Release\SAMPR.exe"
$tests = "$root\build\SAMPR_Tests_artefacts\Release\SAMPR_Tests.exe"

if (-not (Test-Path $app)) { throw "Missing app binary: $app" }
if (-not (Test-Path $tests)) { throw "Missing test binary: $tests" }

Write-Host "== Unit + smoke tests =="
& $tests
if ($LASTEXITCODE -ne 0) { throw "SAMPR_Tests failed with exit code $LASTEXITCODE" }

$appInfo = Get-Item $app
Write-Host "== App artifact =="
Write-Host ("SAMPR.exe {0:N0} bytes @ {1}" -f $appInfo.Length, $appInfo.LastWriteTime)

Write-Host "All smoke checks passed."
Write-Host ""
Write-Host "Desktop launch:  $root\launch.bat"
Write-Host "Or from repo root: $root\..\launch.bat"