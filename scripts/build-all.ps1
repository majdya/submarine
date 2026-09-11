# build-all.ps1 - build every part of the submarine project.
# Run from anywhere:  .\scripts\build-all.ps1  [-Clean] [-SkipFirmware] [-SkipUi] [-Release]
#
#   firmware/        STM32L4 target — CMake + Ninja + arm-none-eabi,
#                    all three borrowed from the STM32CubeIDE install.
#   CMakeLists.txt   root project — central-computer/ + ground_station/
#   web-ui/          Vite/React dashboard — npm

param(
    [switch]$Clean,
    [switch]$SkipFirmware,
    [switch]$SkipUi,
    [switch]$Release
)

$root        = Split-Path $PSScriptRoot -Parent
$BuildDir    = Join-Path $root "build"
$FirmwareDir = Join-Path $root "firmware"
$UiDir       = Join-Path $root "web-ui"
$Preset      = if ($Release) { "Release" } else { "Debug" }

Set-Location $root

Write-Host "=== Submarine build ===" -ForegroundColor Green
Write-Host "root:   $root"
Write-Host "config: $Preset"
Write-Host ""

if ($Clean) {
    Write-Host "Cleaning..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
    $fwBuild = Join-Path $FirmwareDir "build"
    if (Test-Path $fwBuild) { Remove-Item -Recurse -Force $fwBuild }
    Write-Host "Clean complete. Run again without -Clean to build." -ForegroundColor Green
    exit 0
}

$results = @()

# --------------------------------------------------------------------------
# STM32CubeIDE bundles arm-none-eabi-gcc, ninja and cmake inside its plugins\
# folder. None of them are on PATH, which is why a bare `cmake` on the
# firmware picks MSVC and CMSIS then dies with
#   cmsis_compiler.h(278): error C1189: #error: Unknown compiler
# Borrow the bundled ones for this process only.
# --------------------------------------------------------------------------
function Import-CubeIdeToolchain {
    # Resolve the plugins folder FIRST, then list it. Do not hand a path that
    # still contains a wildcard to Get-ChildItem -Filter: it quietly matches
    # nothing, .FullName on the resulting $null becomes "", and the caller ends
    # up prepending a bare "\tools\bin" to PATH - which looks like it worked
    # right up until every tool reports "not recognized".
    $pluginRoots = @(
        "C:\ST\STM32CubeIDE_*\STM32CubeIDE\plugins",
        "$env:ProgramFiles\STMicroelectronics\STM32CubeIDE*\STM32CubeIDE\plugins",
        "${env:ProgramFiles(x86)}\STMicroelectronics\STM32CubeIDE*\STM32CubeIDE\plugins"
    ) | ForEach-Object { Resolve-Path $_ -ErrorAction SilentlyContinue } |
        ForEach-Object { $_.Path }

    if (-not $pluginRoots) { return $null }

    # Newest plugin version wins when several are installed side by side.
    function Newest($like) {
        $pluginRoots |
            ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory -ErrorAction SilentlyContinue } |
            Where-Object { $_.Name -like $like } |
            Sort-Object Name -Descending | Select-Object -First 1
    }

    # Every one of these is verified by locating the actual executable, so a
    # renamed or restructured plugin fails here with a clear message instead of
    # surfacing later as a confusing CMake error.
    function BinOf($plugin, $exe) {
        if (-not $plugin) { return $null }
        $bin = Join-Path $plugin.FullName "tools\bin"
        if (Test-Path (Join-Path $bin $exe)) { return $bin }
        return $null
    }

    $gccBin   = BinOf (Newest "*externaltools.gnu-tools-for-stm32*") "arm-none-eabi-gcc.exe"
    $ninjaBin = BinOf (Newest "*externaltools.ninja.win32*")         "ninja.exe"
    $cmakeBin = BinOf (Newest "*externaltools.cmake.win32*")         "cmake.exe"

    if (-not $gccBin)   { return $null }
    if (-not $ninjaBin) { return $null }

    $added = @($gccBin, $ninjaBin) + @($cmakeBin | Where-Object { $_ })
    $env:PATH = ($added -join ";") + ";" + $env:PATH

    return [pscustomobject]@{
        Paths     = $added
        NinjaExe  = (Join-Path $ninjaBin "ninja.exe")
    }
}

# ---------------------------------------------------------------- 1. firmware
Write-Host "[1/3] firmware (STM32L4)" -ForegroundColor Yellow
if ($SkipFirmware) {
    Write-Host "  -  skipped (-SkipFirmware)" -ForegroundColor DarkGray
    $results += ,@("firmware", "skipped")
}
elseif (-not (Test-Path (Join-Path $FirmwareDir "CMakePresets.json"))) {
    Write-Host "  x  firmware\CMakePresets.json not found" -ForegroundColor Red
    $results += ,@("firmware", "failed")
}
else {
    $tools = Import-CubeIdeToolchain
    if (-not $tools) {
        Write-Host "  !  STM32CubeIDE's bundled ARM toolchain not found." -ForegroundColor Yellow
        Write-Host "     Build the firmware in the IDE, or install the ARM GNU toolchain." -ForegroundColor Yellow
        $results += ,@("firmware", "manual")
    }
    else {
        foreach ($t in $tools) { Write-Host "  PATH += $t" -ForegroundColor DarkGray }

        $gccVer = (& arm-none-eabi-gcc --version 2>$null | Select-Object -First 1)
        if ($gccVer) { Write-Host "  $gccVer" -ForegroundColor DarkGray }

        Push-Location $FirmwareDir
        try {
            cmake --preset $Preset | Out-Host
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  x  configure failed" -ForegroundColor Red
                $results += ,@("firmware", "failed")
            }
            else {
                cmake --build --preset $Preset | Out-Host
                if ($LASTEXITCODE -ne 0) {
                    Write-Host "  x  compile failed" -ForegroundColor Red
                    $results += ,@("firmware", "failed")
                } else {
                    Write-Host "  ok firmware built -> firmware\build\$Preset\" -ForegroundColor Green
                    $results += ,@("firmware", "ok")
                }
            }
        } finally { Pop-Location }
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
        # central-computer compiles the firmware's own tlv.c / comm_frame.c, so
        # it needs to know where the firmware tree is. Passed explicitly because
        # FIRMWARE_DIR is a CACHE entry - an old build\ would otherwise keep
        # pointing at wherever the firmware used to live.
        cmake $root "-DFIRMWARE_DIR=$FirmwareDir" | Out-Host
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
    Write-Host "  !  npm not on PATH - install Node.js LTS to build the dashboard:" -ForegroundColor Yellow
    Write-Host "     https://nodejs.org  (or: winget install OpenJS.NodeJS.LTS)" -ForegroundColor Yellow
    Write-Host "     The C++ side builds fine without it." -ForegroundColor DarkGray
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
Write-Host "Artifacts:" -ForegroundColor Cyan
@(
    (Join-Path $BuildDir "central-computer"),
    (Join-Path $BuildDir "ground_station"),
    (Join-Path $FirmwareDir "build")
) | Where-Object { Test-Path $_ } | ForEach-Object {
    Get-ChildItem -Path $_ -Recurse -Include *.exe, *.elf, *.bin, *.hex -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch "^CompilerId" -and $_.FullName -notmatch "CMakeFiles" } |
        ForEach-Object { Write-Host "  $($_.FullName.Substring($root.Length + 1))" }
}

Write-Host ""
Write-Host "Start the PC side with:  .\scripts\start-all.ps1" -ForegroundColor Cyan
