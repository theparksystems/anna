# ANNA post-deploy e2e smoke tests
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$assistantDir = Join-Path $root "assistant"
$assistantPort = 3921
$assistantUrl = "http://127.0.0.1:$assistantPort"
$buildDir = Join-Path (Split-Path -Parent $root) "build-anna"
$app = Join-Path $buildDir "ANNA_artefacts\Release\ANNA.exe"
$tests = Join-Path $buildDir "ANNA_Tests_artefacts\Release\ANNA_Tests.exe"
$failures = @()
$assistantProc = $null

function Pass([string]$msg) { Write-Host "[PASS] $msg" -ForegroundColor Green }
function Fail([string]$msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red; $script:failures += $msg }
function Info([string]$msg) { Write-Host "[INFO] $msg" -ForegroundColor Cyan }

function Test-AssistantHealth {
    try {
        $resp = Invoke-RestMethod -Uri "$assistantUrl/health" -TimeoutSec 3
        return $resp.status -eq "ok"
    } catch { return $false }
}

function Stop-AssistantIfRunning {
    if ($null -ne $script:assistantProc -and -not $script:assistantProc.HasExited) {
        Stop-Process -Id $script:assistantProc.Id -Force -ErrorAction SilentlyContinue
        $script:assistantProc = $null
    }
    Get-Process node -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -like "*\anna\assistant\*" } catch { $false }
    } | Stop-Process -Force -ErrorAction SilentlyContinue
}

function Start-AssistantForTests {
    if (Test-AssistantHealth) {
        Info "Assistant already running"
        return $true
    }

    if (-not (Test-Path (Join-Path $assistantDir "server.js"))) {
        Fail "Assistant server.js missing"
        return $false
    }

    if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
        Fail "Node.js not installed"
        return $false
    }

    if (-not (Test-Path (Join-Path $assistantDir "node_modules"))) {
        Info "Installing assistant dependencies..."
        Push-Location $assistantDir
        & npm install --no-fund --no-audit | Out-Host
        Pop-Location
    }

    if (-not (Test-Path (Join-Path $assistantDir ".env"))) {
        Copy-Item (Join-Path $assistantDir ".env.example") (Join-Path $assistantDir ".env")
    }

    $script:assistantProc = Start-Process -FilePath "node" -ArgumentList "server.js" `
        -WorkingDirectory $assistantDir -PassThru -WindowStyle Hidden

    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) {
        if (Test-AssistantHealth) { return $true }
        Start-Sleep -Milliseconds 300
    }

    Fail "Assistant did not become healthy on port $assistantPort"
    return $false
}

function Get-AvailableGemmaModel {
    if (-not (Get-Command ollama -ErrorAction SilentlyContinue)) { return $null }
    $list = & ollama list 2>$null
    if ($list -match "gemma2:9b") { return "gemma2:9b" }
    if ($list -match "gemma:7b") { return "gemma:7b" }
    if ($list -match "gemma3:4b") { return "gemma3:4b" }
    if ($list -match "gemma3:12b") { return "gemma3:12b" }
    if ($list -match "gemma2") { return "gemma2" }
    return $null
}

Write-Host "========================================" -ForegroundColor Yellow
Write-Host " ANNA Post-Deploy E2E Smoke Tests" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow

# 1) Build + unit tests
Info "Phase 1: Build and unit tests"
& (Join-Path $PSScriptRoot "run_smoke.ps1")
if ($LASTEXITCODE -ne 0) {
    Fail "run_smoke.ps1 failed"
} else {
    Pass "Build and unit tests"
}

# 2) Artifact checks
Info "Phase 2: Artifact verification"
if (Test-Path $app) {
    $size = (Get-Item $app).Length
    if ($size -gt 1MB) { Pass "ANNA.exe exists ($([math]::Round($size/1MB,1)) MB)" }
    else { Fail "ANNA.exe too small ($size bytes)" }
} else {
    Fail "ANNA.exe missing"
}

# 3) Assistant sidecar
Info "Phase 3: Assistant sidecar API"
try {
    if (Start-AssistantForTests) {
        $health = Invoke-RestMethod -Uri "$assistantUrl/health" -TimeoutSec 5
        Pass "Assistant /health -> $($health.status) model=$($health.model)"

        $stats = Invoke-RestMethod -Uri "$assistantUrl/api/memory/stats" -TimeoutSec 5
        if ($null -ne $stats.short -and $null -ne $stats.medium) {
            Pass "Assistant /api/memory/stats (short=$($stats.short) medium=$($stats.medium) long=$($stats.long))"
        } else {
            Fail "Assistant memory stats malformed"
        }

        $search = Invoke-RestMethod -Uri "$assistantUrl/api/memory/search?q=flat" -TimeoutSec 5
        if ($search -is [array]) {
            Pass "Assistant /api/memory/search?q=flat ($($search.Count) hits)"
        } else {
            Fail "Assistant memory search failed"
        }
    }
} catch {
    Fail "Assistant API error: $($_.Exception.Message)"
}

# 4) Assistant chat round-trip (requires Ollama + a Gemma model)
Info "Phase 4: Assistant chat round-trip (Ollama)"
$gemmaModel = Get-AvailableGemmaModel
if ($null -eq $gemmaModel) {
    Fail "No Gemma model in Ollama - run: ollama pull gemma2:9b"
} elseif (-not (Test-AssistantHealth)) {
    Fail "Assistant offline - skipping chat test"
} else {
    Info "Using Ollama model: $gemmaModel"
    try {
        Stop-AssistantIfRunning
        Start-Sleep -Milliseconds 500
        $env:OLLAMA_MODEL = $gemmaModel
        $script:assistantProc = Start-Process -FilePath "node" -ArgumentList "server.js" `
            -WorkingDirectory $assistantDir -PassThru -WindowStyle Hidden
        $deadline = (Get-Date).AddSeconds(10)
        while ((Get-Date) -lt $deadline) {
            if (Test-AssistantHealth) { break }
            Start-Sleep -Milliseconds 300
        }
        if (-not (Test-AssistantHealth)) {
            Fail "Assistant restart for chat test failed"
        } else {
            Push-Location $assistantDir
            & node test/smoke.js
            if ($LASTEXITCODE -eq 0) { Pass "Assistant chat round-trip ($gemmaModel)" }
            else { Fail "Assistant chat smoke.js exit code $LASTEXITCODE" }
            Pop-Location
        }
    } catch {
        Fail "Assistant chat error: $($_.Exception.Message)"
    }
}

# 5) ANNA process launch smoke
Info "Phase 5: ANNA process launch"
if (Test-Path $app) {
    try {
        $proc = Start-Process -FilePath $app -WorkingDirectory (Split-Path $app) -PassThru
        Start-Sleep -Seconds 3
        if (-not $proc.HasExited) {
            Pass "ANNA.exe launched and stayed alive (pid $($proc.Id))"
            Stop-Process -Id $proc.Id -Force
        } else {
            Fail "ANNA.exe exited early with code $($proc.ExitCode)"
        }
    } catch {
        Fail "ANNA launch error: $($_.Exception.Message)"
    }
} else {
    Fail "Cannot launch - ANNA.exe missing"
}

# 6) Integration static checks (tabs / assistant wiring)
Info "Phase 6: Integration static checks"
$mainCpp = Join-Path $root "Source\Shell\MainComponent.cpp"
$mainSrc = Get-Content $mainCpp -Raw
$tabChecks = @(
    @{ Name = "Step Seq tab"; Pattern = 'Step Seq \(1\)' },
    @{ Name = "Piano Roll tab"; Pattern = 'Piano Roll \(2\)' },
    @{ Name = "Arrangement tab"; Pattern = 'Arrangement \(3\)' },
    @{ Name = "FX Rack tab"; Pattern = 'FX Rack \(4\)' },
    @{ Name = "Assistant tab"; Pattern = 'Assistant \(5\)' },
    @{ Name = "Ask Gemma toolbar"; Pattern = 'Ask Gemma' },
    @{ Name = "Mixer Ask button"; Pattern = 'setAskCallback' },
    @{ Name = "FX Ask button"; Pattern = 'fxWorkspace\.setAskCallback' }
)
foreach ($check in $tabChecks) {
    if ($mainSrc -match $check.Pattern) { Pass $check.Name }
    else { Fail "$($check.Name) not found in MainComponent.cpp" }
}

$launchPs1 = Get-Content (Join-Path $root "launch.ps1") -Raw
if ($launchPs1 -match 'Start-AssistantSidecar') { Pass "launch.ps1 starts assistant sidecar" }
else { Fail "launch.ps1 missing assistant startup" }

Stop-AssistantIfRunning

Write-Host ""
Write-Host "========================================" -ForegroundColor Yellow
if ($failures.Count -eq 0) {
    Write-Host " ALL E2E SMOKE TESTS PASSED ($((Get-Date).ToString('HH:mm:ss')))" -ForegroundColor Green
    exit 0
}

Write-Host " E2E SMOKE FAILURES: $($failures.Count)" -ForegroundColor Red
foreach ($f in $failures) { Write-Host "  - $f" -ForegroundColor Red }
exit 1
