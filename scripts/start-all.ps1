# start-all.ps1 - launch the PC-side services (Central Computer + Ground Station).
# Lives in scripts\ and resolves the project root itself:
#   .\scripts\start-all.ps1            # each service in its own window
#   .\scripts\start-all.ps1 -Stop      # stop whatever this script started

param(
    [switch]$Stop
)

$root     = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $root "build"
$PidFile  = Join-Path $BuildDir ".running-pids"

Set-Location $root

# ---------- stop mode ----------
if ($Stop) {
    if (-not (Test-Path $PidFile)) {
        Write-Host "Nothing recorded as running." -ForegroundColor Yellow
        exit 0
    }
    Get-Content $PidFile | ForEach-Object {
        $procId = [int]$_
        $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
        if ($p) {
            Stop-Process -Id $procId -Force
            Write-Host "  stopped $($p.ProcessName) (PID $procId)" -ForegroundColor Green
        }
    }
    Remove-Item $PidFile -Force
    Write-Host "All services stopped." -ForegroundColor Green
    exit 0
}

# ---------- locate binaries (Debug or Release, either CMake layout) ----------
function Find-Exe($subdir, $name) {
    $candidates = @(
        (Join-Path $BuildDir "$subdir\Debug\$name.exe"),
        (Join-Path $BuildDir "$subdir\Release\$name.exe"),
        (Join-Path $BuildDir "$subdir\$name.exe")
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    # last resort: search the subtree
    $hit = Get-ChildItem -Path (Join-Path $BuildDir $subdir) -Filter "$name.exe" -Recurse -ErrorAction SilentlyContinue |
           Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return $null
}

$central = Find-Exe "central-computer" "central_computer"
$ground  = Find-Exe "ground_station"   "ground_station"

$missing = @()
if (-not $central) { $missing += "central_computer" }
if (-not $ground)  { $missing += "ground_station" }
if ($missing.Count -gt 0) {
    Write-Host "Not built yet: $($missing -join ', ')" -ForegroundColor Red
    Write-Host "Run:  .\scripts\build-all.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "=== Starting submarine services ===" -ForegroundColor Green
Write-Host ""

$pids = @()

# Central Computer first - Ground Station connects to it over TCP
Write-Host "Central Computer ..." -ForegroundColor Yellow
$p1 = Start-Process -FilePath $central -PassThru
$pids += $p1.Id
Write-Host "  ok PID $($p1.Id)  ($($central.Substring($root.Length + 1)))" -ForegroundColor Green

Start-Sleep -Seconds 2

Write-Host "Ground Station ..." -ForegroundColor Yellow
$p2 = Start-Process -FilePath $ground -PassThru
$pids += $p2.Id
Write-Host "  ok PID $($p2.Id)  ($($ground.Substring($root.Length + 1)))" -ForegroundColor Green

$pids | Set-Content $PidFile

Write-Host ""
Write-Host "=== Running ===" -ForegroundColor Green
Write-Host "  Central Computer  PID $($p1.Id)   (UART to the Nucleo, TCP to Ground Station)"
Write-Host "  Ground Station    PID $($p2.Id)   (operator console)"
Write-Host ""
Write-Host "Stop them with:  .\scripts\start-all.ps1 -Stop" -ForegroundColor Cyan
