# build-all.ps1 - build every part of the submarine project.
# Run from anywhere:  .\scripts\build-all.ps1  [-Clean] [-SkipFirmware] [-SkipUi]
#
# Layout it expects:
#   firmware/          STM32L4 target      -> STM32CubeIDE (has its own ARM toolchain)
#   CMakeLists.txt     root project        -> central-computer/ + ground_station/
#   web-ui/            Vite/React dashboard-> npm

param(
    [switch]$Clean,
    [switch]$SkipFirmware,
    [switch]$SkipUi
)

$root        = Split-Path $PSScriptRoot -Parent
$BuildDir    = Join-Path $root "build"
$FirmwareDir = Join-Path $root "firmware"
$UiDir       = Join-Path $root "web-ui"

Set-Location $root

Write-Host "=== Submarine build ===" -ForegroundColor Green
Write-Host "root: $root"
Write-Host ""

if ($Clean) {
    Write-Host "Cleaning build\ ..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    Write-Host "Clean complete. Run again without -Clean to build." -ForegroundColor Green
    exit 0
}

$results = @()

# ---------------------------------------------------------------- 1. firmware
Write-Host "[1/3] firmware (STM32L4)" -ForegroundColor Yellow
if ($SkipFirmware) {
    Write-Host "  -  skipped (-SkipFirmware)" -ForegroundColor DarkGray
    $results += ,@("firmware", "skipped")
}
elseif (-not (Test-Path $FirmwareDir)) {
    Write-Host "  x  firmware\ not found" -ForegroundColor Red
    $results += ,@("firmware", "failed")
}
else {
    # NOTE: do NOT try plain `cmake` here. arm-none-eabi-gcc is not on PATH on
    # this machine, so CMake picks MSVC and CMSIS fails with
    # "cmsis_compiler.h(278): error C1189: #error: Unknown compiler".
    # STM32CubeIDE ships its own ARM toolchain - use it.
    $ide = @(
        "C:\ST\STM32CubeIDE_*\STM32CubeIDE\stm32cubeide.exe",
        "$env:ProgramFiles\STMicroelectronics\STM32CubeIDE*\STM32CubeIDE\stm32cubeide.exe",
        "${env:ProgramFiles(x86)}\STMicroelectronics\STM32CubeIDE*\STM32CubeIDE\stm32cubeide.exe"
    ) | ForEach-Object { Get-Item $_ -ErrorAction SilentlyContinue } |
        Sort-Object FullName -Descending | Select-Object -First 1

    if (-not $ide) {
        Write-Host "  !  stm32cubeide.exe not found - build it in the IDE:" -ForegroundColor Yellow
        Write-Host "     File > Import > Existing Projects into Workspace > firmware\, then Build Project"
        $results += ,@("firmware", "manual")
    }
    else {
        # Eclipse's project name lives in .project, not in the folder name.
        $projName   = "firmware"
        $dotProject = Join-Path $FirmwareDir ".project"
        if (Test-Path $dotProject) {
            try { $projName = ([xml](Get-Content $dotProject)).projectDescription.name } catch { }
        }
        $ws = Join-Path $BuildDir "cubeide-workspace"
        Write-Host "  ide:     $($ide.FullName)" -ForegroundColor DarkGray
        Write-Host "  project: $projName (headless build, ~1 min)" -ForegroundColor DarkGray

        & $ide.FullName --launcher.suppressErrors -nosplash `
            -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
            -data $ws -import $FirmwareDir -cleanBuild "$projName/Debug" | Out-Host

        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ok firmware built" -ForegroundColor Green
            $results += ,@("firmware", "ok")
        } else {
            Write-Host "  x  headless build failed - build it in the IDE instead" -ForegroundColor Red
            $results += ,@("firmware", "failed")
        }
    }
}
Write-Host ""

# ------------------------------------------- 2. PC apps (one root CMake project)
Write-Host "[2/3] central-computer + ground_station" -ForegroundColor Yellow
if (-not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
    Write-Host "  x  root CMakeLists.txt not found" -ForegroundColor Red
    $results += ,@("pc-apps", "failed")
}
else {
    if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }
    Push-Location $BuildDir
    try {
        cmake $root | Out-Host
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  x  cmake configure failed" -ForegroundColor Red
            $results += ,@("pc-apps", "failed")
        }
        else {
            cmake --build . --parallel $env:NUMBER_OF_PROCESSORS | Out-Host
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  x  compile failed" -ForegroundColor Red
                $results += ,@("pc-apps", "failed")
            } else {
                Write-Host "  ok central-computer + ground_station built" -ForegroundColor Green
                $results += ,@("pc-apps", "ok")
            }
        }
    } finally { Pop-Location }
}
Write-Host ""

# ---------------------------------------------------------------- 3. web-ui
Write-Host "[3/3] web-ui (Vite dashboard)" -ForegroundColor Yellow
if ($SkipUi) {
    Write-Host "  -  skipped (-SkipUi)" -ForegroundColor DarkGray
    $results += ,@("web-ui", "skipped")
}
elseif (-not (Test-Path (Join-Path $UiDir "package.json"))) {
    Write-Host "  -  web-ui\package.json not found - skipped" -ForegroundColor DarkGray
    $results += ,@("web-ui", "skipped")
}
elseif (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
    Write-Host "  !  npm not on PATH - install Node.js to build the dashboard" -ForegroundColor Yellow
    $results += ,@("web-ui", "manual")
}
else {
    Push-Location $UiDir
    try {
        if (-not (Test-Path "node_modules")) {
            Write-Host "  installing dependencies (first run)..." -ForegroundColor DarkGray
            npm install | Out-Host
        }
        npm run build | Out-Host
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ok web-ui built -> web-ui\dist\" -ForegroundColor Green
            $results += ,@("web-ui", "ok")
        } else {
            Write-Host "  x  npm run build failed" -ForegroundColor Red
            $results += ,@("web-ui", "failed")
        }
    } finally { Pop-Location }
}
Write-Host ""

# ---------------------------------------------------------------- summary
Write-Host "=== Summary ===" -ForegroundColor Green
foreach ($r in $results) {
    $color = switch ($r[1]) { "ok" { "Green" } "failed" { "Red" } default { "Yellow" } }
    Write-Host ("  {0,-18} {1}" -f $r[0], $r[1]) -ForegroundColor $color
}

Write-Host ""
Write-Host "Binaries:" -ForegroundColor Cyan
Get-ChildItem -Path $BuildDir -Recurse -Include *.exe, *.elf -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch "cubeide-workspace" } |
    ForEach-Object { Write-Host "  $($_.FullName.Substring($root.Length + 1))" }

Write-Host ""
Write-Host "Start the PC side with:  .\scripts\start-all.ps1" -ForegroundColor Cyan
