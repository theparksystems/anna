# ANNA post-build smoke tests
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path (Split-Path -Parent $root) "build-anna"

function Find-CMake {
    $fromPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) { return $fromPath.Source }

    $candidates = @(
        "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
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
    $fromPath = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) { return $fromPath.Source }

    $candidates = @(
        "C:\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }

    return $null
}

function Find-Ninja {
    $fromPath = Get-Command ninja -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) { return $fromPath.Source }

    $candidate = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (Test-Path $candidate) { return $candidate }

    return $null
}

function Find-CL {
    $candidate = "C:\BuildTools\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe"
    if (Test-Path $candidate) { return $candidate }

    $fromPath = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) { return $fromPath.Source }

    $candidate = "C:\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"
    if (Test-Path $candidate) { return $candidate }

    return $null
}

function Find-WindowsSdkTool([string] $toolName) {
    $fromPath = Get-Command $toolName -ErrorAction SilentlyContinue
    if ($null -ne $fromPath) { return $fromPath.Source }

    $candidate = Join-Path "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64" $toolName
    if (Test-Path $candidate) { return $candidate }

    return $null
}

function Import-VsBuildEnv {
    $vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { return }

    $envLines = cmd /c "`"$vcvars`" >nul && set"
    foreach ($line in $envLines) {
        $idx = $line.IndexOf('=')
        if ($idx -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $idx), $line.Substring($idx + 1), 'Process')
        }
    }
}

Import-VsBuildEnv

$cmake = Find-CMake
$msbuild = Find-MSBuild
$ninja = Find-Ninja
$cl = Find-CL
$rc = Find-WindowsSdkTool "rc.exe"
$mt = Find-WindowsSdkTool "mt.exe"

if ($null -eq $cmake) { throw "CMake not found. Install Visual Studio Build Tools with the Desktop development with C++ workload, or install CMake and add it to PATH." }
if ($null -eq $ninja) { throw "Ninja not found. Install Visual Studio Build Tools with the Desktop development with C++ workload." }
if ($null -eq $cl) { throw "MSVC compiler not found. Install Visual Studio Build Tools with the Desktop development with C++ workload." }
if ($null -eq $rc) { throw "Windows SDK resource compiler not found. Install the Windows SDK component for Visual Studio Build Tools." }
if ($null -eq $mt) { throw "Windows SDK manifest tool not found. Install the Windows SDK component for Visual Studio Build Tools." }

Write-Host "== Configure =="
$ninjaArg = $ninja.Replace('\', '/')
$clArg = $cl.Replace('\', '/')
$rcArg = $rc.Replace('\', '/')
$mtArg = $mt.Replace('\', '/')
& $cmake -S $root -B $buildDir -G Ninja -DCMAKE_MAKE_PROGRAM="$ninjaArg" -DCMAKE_CXX_COMPILER="$clArg" -DCMAKE_C_COMPILER="$clArg" -DCMAKE_RC_COMPILER="$rcArg" -DCMAKE_MT="$mtArg" -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DANNA_ENABLE_RUBBERBAND=ON | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Configure failed with exit code $LASTEXITCODE" }

Write-Host "== Build Release =="
& $cmake --build $buildDir --config Release | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

$app = "$buildDir\ANNA_artefacts\Release\ANNA.exe"
$tests = "$buildDir\ANNA_Tests_artefacts\Release\ANNA_Tests.exe"

if (-not (Test-Path $app)) { throw "Missing app binary: $app" }
if (-not (Test-Path $tests)) { throw "Missing test binary: $tests" }

Write-Host "== Unit + smoke tests =="
& $tests
if ($LASTEXITCODE -ne 0) { throw "ANNA_Tests failed with exit code $LASTEXITCODE" }

$appInfo = Get-Item $app
Write-Host "== App artifact =="
Write-Host ("ANNA.exe {0:N0} bytes @ {1}" -f $appInfo.Length, $appInfo.LastWriteTime)

Write-Host "All smoke checks passed."
Write-Host ""
Write-Host "Desktop launch:  $root\launch.bat"
Write-Host "Or from repo root: $root\..\launch.bat"
