# BTHL-SpiritBox — Windows Port Guide

This branch (`windows-dev`) carries the groundwork for a native Windows (x64) build. The macOS
build is untouched — every Windows-specific change is guarded by `if(WIN32)` in CMake or compiled
only on non-Apple platforms. Clone this branch on your Windows machine and follow the steps below.

> Status: **builds-ready scaffolding**, not yet compiled on Windows. The cross-platform core
> (Qt UI, SoapySDR sweep, PortAudio, Qt SerialPort EMF reader, whisper.cpp) is platform-neutral.
> The only macOS-only subsystem (the Wi-Fi/Bluetooth ambient scanner) is replaced by a no-op stub
> on Windows, so the app links and runs — that sensor panel simply reports "unavailable" until a
> native backend is written (see TODO #1).

---

## 1. What this branch already did

- **`src/sensors/WirelessScanner_stub.cpp`** — no-op `WirelessScanner` for non-Apple so the app
  links and runs. (macOS keeps `WirelessScanner.mm` / CoreWLAN + CoreBluetooth.)
- **CMake cross-platform**
  - PortAudio discovery now falls back from pkg-config to a vcpkg/`find_package` path on Windows.
  - Non-Apple builds compile the wireless stub instead of the `.mm`.
  - `if(WIN32)` block: configures `resources/windows/spiritbox.rc.in` (app icon + version info),
    builds a **GUI-subsystem** `.exe` (no console window), links **`Qt6::EntryPoint`** (WinMain),
    runs **windeployqt** after the build, and copies `docs/` next to the `.exe` for the in-app help.
- **`resources/windows/spiritbox.rc.in`** — embeds `assets/icons/app.ico` + version metadata.
- **`scripts/build-windows.ps1`** / **`scripts/package-windows.ps1`** — one-command configure+build
  and a portable-zip packager.
- Generated raster assets (`app.ico`, `icon-512.png`, `splash.png`) are committed on this branch so
  a fresh clone builds without the rasterization toolchain (rsvg/ImageMagick).

---

## 2. Prerequisites

| Tool | Notes |
|------|-------|
| **Visual Studio 2022** (Desktop C++) or Build Tools | MSVC v143, Windows 10/11 SDK |
| **CMake ≥ 3.21** | bundled with VS, or standalone |
| **Ninja** | fast generator (optional; VS generator also works) |
| **Git** | for the clone + whisper.cpp submodule |
| **Qt 6.x for MSVC** (with **Qt WebEngine**) | official online installer or `aqtinstall`. Needs Widgets, Multimedia, SerialPort, Charts, Concurrent, Network, **WebEngineWidgets** |
| **vcpkg** | for SoapySDR + PortAudio (or use PothosSDR for the SDR stack — see below) |

### Dependencies via vcpkg
```powershell
git clone https://github.com/microsoft/vcpkg C:\src\vcpkg
C:\src\vcpkg\bootstrap-vcpkg.bat
C:\src\vcpkg\vcpkg install portaudio:x64-windows soapysdr:x64-windows
```

### SDR drivers — recommended: PothosSDR
The cleanest Windows SDR stack is **PothosSDR** (https://github.com/pothosware/PothosSDR/wiki),
which ships prebuilt **SoapySDR + SoapyRTLSDR + librtlsdr** and the **Zadig** WinUSB driver tool.
Install it, then either point `CMAKE_PREFIX_PATH` at it or let vcpkg provide SoapySDR and use
PothosSDR only for the runtime driver modules. Use **Zadig** to bind the RTL-SDR dongle to WinUSB.

### whisper.cpp (optional but recommended)
```powershell
git clone https://github.com/ggerganov/whisper.cpp.git lib\whisper.cpp
```
Absent → the build still succeeds with transcription disabled (`WHISPER_AVAILABLE=0`).

---

## 3. Build

```powershell
git clone -b windows-dev <repo-url> bthl-spiritbox
cd bthl-spiritbox
git clone https://github.com/ggerganov/whisper.cpp.git lib\whisper.cpp   # optional

powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1 `
    -QtDir C:\Qt\6.11.1\msvc2022_64 `
    -VcpkgRoot C:\src\vcpkg
```
The build stages the Qt runtime (windeployqt) and `docs\` next to `build\BTHL-SpiritBox.exe`.
Run it from that folder. Package a portable zip with:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1
```

---

## 4. Runtime notes (Windows specifics)

- **RTL-SDR**: bind the dongle to **WinUSB** with Zadig, or SoapySDR won't enumerate it.
- **GQ EMF-390**: the CH340 USB-serial bridge appears as a **COMx** port. Qt SerialPort enumerates
  it cross-platform; install the CH340 driver if the device shows as unknown. Pick the COM port in
  the EMF Monitor panel exactly as you'd pick `cu.usbserial-*` on macOS.
- **Microphone (Interactive mode)**: Windows gates mic access in *Settings → Privacy → Microphone*.
  Allow desktop apps. (There's no Info.plist analog — the OS prompt/allow-list governs it.)
- **Single-instance lock** uses `QLockFile` in the temp dir — already cross-platform, no change.

---

## 5. Platform TODO checklist (for "take it from there")

1. **WirelessScanner native backend** — replace `WirelessScanner_stub.cpp` on Win32:
   - Wi-Fi: **Native Wifi API** (`wlanapi.h`: `WlanOpenHandle` → `WlanGetNetworkBssList`) for AP
     count + best RSSI; link `wlanapi.lib`.
   - Bluetooth-LE: **WinRT** `BluetoothLEAdvertisementWatcher` for BLE device count + best RSSI
     (C++/WinRT). Emit `WirelessSnapshot` on the existing `snapshot(...)` signal and the UI lights up.
2. **In-app updater artifact** — `UpdateChecker` reads `updates.php` → `pkg_url` (a macOS `.pkg`).
   For Windows it must fetch the Windows artifact (`.exe`/`.zip`). Plan: have `updates.php` return a
   per-platform URL (e.g. `?platform=win64`, or `win_url` alongside `pkg_url`) and have
   `UpdateChecker` request/select the right one. `BthlEndpoints.h` is the single place for URLs.
   (The **model** downloader is platform-agnostic `.bin` files — no change needed.)
3. **Code signing** — sign `BTHL-SpiritBox.exe` (and the installer) with `signtool` + an
   Authenticode cert once available. Hook it into `package-windows.ps1`.
4. **Installer** — author `installer\windows\spiritbox.iss` (Inno Setup) or a WiX project;
   `package-windows.ps1` already detects `iscc.exe` and is ready to invoke it.
5. **Icon sanity** — confirm `app.ico` embeds via the `.rc` (taskbar + Explorer). If you regenerate
   art, `assets/branding/generate-assets.sh` rebuilds `app.ico` (needs ImageMagick; runs under
   Git Bash/WSL on Windows).
6. **QtWebEngine deploy** — windeployqt copies `QtWebEngineProcess.exe` + resources; verify the
   in-app docs browser renders. If it's blank, check the `resources`/`translations` folders landed
   next to the exe.
7. **Smoke test** — launch, open an RTL-SDR, run an FM sweep, load `ggml-base.en.bin`, confirm
   transcriptions; connect the EMF-390 on its COM port; open **Help → Documentation**.

---

## 6. Cross-platform parts that need no work

Qt Widgets UI, the SoapySDR sweep engine, PortAudio output, the Qt SerialPort EMF reader, the
session recorder (WAV/JSONL/CSV), the VAD, whisper.cpp transcription, the model downloader, and the
`QLockFile` single-instance guard are all platform-neutral and build as-is.
