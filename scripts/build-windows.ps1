<#
.SYNOPSIS
    Configure and build BTHL-SpiritBox on Windows (x64).

.DESCRIPTION
    We wrap the CMake configure+build for Windows so a fresh clone builds with one command.
    Dependencies come from two places:
      - Qt 6 (with WebEngineWidgets) — from the official Qt installer or aqtinstall.
      - SoapySDR + PortAudio + (optionally) the RTL-SDR module — from vcpkg, or from a
        PothosSDR install for the SDR side. See docs/WINDOWS_PORT.md.

    whisper.cpp is built from the lib/whisper.cpp submodule if present (transcription is
    disabled, not fatal, when it is absent).

.PARAMETER QtDir
    Path to the Qt kit, e.g. C:\Qt\6.11.1\msvc2022_64. Falls back to the env var QTDIR / Qt6_DIR.

.PARAMETER VcpkgRoot
    Path to your vcpkg checkout (its scripts\buildsystems\vcpkg.cmake is used as the toolchain).
    Falls back to the env var VCPKG_ROOT. Optional if SoapySDR/PortAudio are findable another way.

.PARAMETER BuildType
    Release (default) or Debug.

.PARAMETER Generator
    CMake generator. Default "Ninja" (fast); pass "Visual Studio 17 2022" if you prefer the IDE.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1 `
        -QtDir C:\Qt\6.11.1\msvc2022_64 -VcpkgRoot C:\src\vcpkg
#>
param(
    [string]$QtDir      = $env:QTDIR,
    [string]$VcpkgRoot  = $env:VCPKG_ROOT,
    [ValidateSet('Release','Debug')][string]$BuildType = 'Release',
    [string]$Generator  = 'Ninja'
)
$ErrorActionPreference = 'Stop'

$Root  = Split-Path -Parent $PSScriptRoot       # repo root (scripts\..)
$Build = Join-Path $Root 'build'

Write-Host "== BTHL-SpiritBox Windows build ==" -ForegroundColor Cyan
Write-Host "  Root      : $Root"
Write-Host "  BuildType : $BuildType"
Write-Host "  Generator : $Generator"

# --- Locate Qt ---------------------------------------------------------------
if (-not $QtDir -and $env:Qt6_DIR) { $QtDir = $env:Qt6_DIR }
if (-not $QtDir) {
    throw "Qt not found. Pass -QtDir C:\Qt\6.x.x\msvc2022_64 (or set QTDIR). See docs/WINDOWS_PORT.md."
}
Write-Host "  Qt        : $QtDir"

# --- Ensure generated branding art exists ------------------------------------
# We keep generated rasters OUT of git and regenerate them from the SVG sources at build time:
# icon-512.png + splash.png (compiled into the .qrc) and app.ico (embedded by the .rc).
# The generator is the shared assets/branding/generate-assets.sh (needs Git Bash + librsvg +
# ImageMagick on Windows; the macOS-only .icns step is skipped automatically). See WINDOWS_PORT.md.
$needArt = @(
    (Join-Path $Root 'assets\icons\icon-512.png'),
    (Join-Path $Root 'assets\branding\splash.png'),
    (Join-Path $Root 'assets\icons\app.ico')
)
if ($needArt | Where-Object { -not (Test-Path $_) }) {
    Write-Host "`n-- Generating branding assets (missing) --" -ForegroundColor Cyan
    $bash = Get-Command bash.exe -ErrorAction SilentlyContinue
    if (-not $bash) {
        throw "Branding assets are missing and 'bash' was not found. Install Git for Windows, plus " +
              "librsvg (rsvg-convert) and ImageMagick (magick) on PATH, then re-run. Generator: " +
              "assets\branding\generate-assets.sh (see docs\WINDOWS_PORT.md)."
    }
    & $bash.Source (Join-Path $Root 'assets\branding\generate-assets.sh')
    if ($LASTEXITCODE -ne 0) {
        throw "generate-assets.sh failed. Ensure rsvg-convert and magick (ImageMagick) are on PATH."
    }
} else {
    Write-Host "  Assets    : present"
}

# --- CMake args --------------------------------------------------------------
$cmakeArgs = @(
    '-S', $Root, '-B', $Build,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_PREFIX_PATH=$QtDir"
)
if ($Generator) { $cmakeArgs += @('-G', $Generator) }

if ($VcpkgRoot) {
    $toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
    if (-not (Test-Path $toolchain)) { throw "vcpkg toolchain not found at $toolchain" }
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    $cmakeArgs += '-DVCPKG_TARGET_TRIPLET=x64-windows'
    Write-Host "  vcpkg     : $VcpkgRoot"
} else {
    Write-Host "  vcpkg     : (none) — SoapySDR/PortAudio must be on CMAKE_PREFIX_PATH" -ForegroundColor Yellow
}

# --- Configure + build -------------------------------------------------------
Write-Host "`n-- Configuring --" -ForegroundColor Cyan
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "`n-- Building --" -ForegroundColor Cyan
& cmake --build $Build --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$exe = Get-ChildItem -Path $Build -Recurse -Filter 'BTHL-SpiritBox.exe' | Select-Object -First 1
Write-Host "`nBUILD OK -> $($exe.FullName)" -ForegroundColor Green
Write-Host "windeployqt and docs were staged next to the .exe by the build. Run it from that folder."
