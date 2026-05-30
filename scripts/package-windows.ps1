<#
.SYNOPSIS
    Package a built BTHL-SpiritBox.exe into a distributable Windows artifact.

.DESCRIPTION
    Assumes scripts\build-windows.ps1 already produced the .exe and that the build's POST_BUILD
    step ran windeployqt + copied docs next to it. We collect the .exe, its Qt runtime, the docs,
    and the bundled base Whisper model into a clean folder and emit:
      - a portable .zip (always), and
      - an Inno Setup installer (.exe) if iscc.exe (Inno Setup) is on PATH.

    Code signing (signtool with an Authenticode cert) is intentionally left as a documented
    manual step in docs/WINDOWS_PORT.md — wire it in once a signing cert is available.

.PARAMETER BuildDir
    The build directory (default: <repo>\build).

.PARAMETER OutDir
    Where to write the staged folder + .zip (default: <repo>\dist).
#>
param(
    [string]$BuildDir,
    [string]$OutDir
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $Root 'build' }
if (-not $OutDir)   { $OutDir   = Join-Path $Root 'dist' }

$version = (Get-Content (Join-Path $BuildDir 'version.txt') -ErrorAction SilentlyContinue)
if (-not $version) { $version = '0.0.0' }
$version = $version.Trim()

$exe = Get-ChildItem -Path $BuildDir -Recurse -Filter 'BTHL-SpiritBox.exe' | Select-Object -First 1
if (-not $exe) { throw "BTHL-SpiritBox.exe not found under $BuildDir. Run build-windows.ps1 first." }
$exeDir = $exe.Directory.FullName

$stage = Join-Path $OutDir "BTHL-SpiritBox-$version-win64"
Write-Host "Staging $stage" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

# Copy everything windeployqt staged next to the exe (DLLs, plugins, QtWebEngine helper, docs).
Copy-Item -Path (Join-Path $exeDir '*') -Destination $stage -Recurse -Force

# Bundle the base Whisper model next to the exe in a user-writable models\ folder the in-app
# downloader can extend (mirrors the macOS sibling-folder layout).
$baseModel = Join-Path $Root 'models\ggml-base.en.bin'
if (Test-Path $baseModel) {
    New-Item -ItemType Directory -Path (Join-Path $stage 'models') -Force | Out-Null
    Copy-Item $baseModel (Join-Path $stage 'models') -Force
    Write-Host "Bundled base model."
} else {
    Write-Host "models\ggml-base.en.bin not present — skipping bundled model (downloader still works)." -ForegroundColor Yellow
}

# Portable zip.
$zip = Join-Path $OutDir "BTHL-SpiritBox-$version-win64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Host "Wrote $zip" -ForegroundColor Green

# Optional Inno Setup installer.
$iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if ($iscc) {
    Write-Host "Inno Setup found — building installer (see installer\windows\spiritbox.iss if present)." -ForegroundColor Cyan
    Write-Host "TODO: author installer\windows\spiritbox.iss and invoke iscc here." -ForegroundColor Yellow
} else {
    Write-Host "Inno Setup (iscc.exe) not on PATH — produced the portable .zip only." -ForegroundColor Yellow
}
