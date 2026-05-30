/**
 * @file src/sensors/WirelessScanner_stub.cpp
 * @title WirelessScanner - non-Apple no-op implementation
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We provide a link-clean, do-nothing WirelessScanner on platforms that do not yet
 *          have a native ambient-Wi-Fi/Bluetooth backend. The macOS backend lives in
 *          WirelessScanner.mm (CoreWLAN + CoreBluetooth); this stub keeps the rest of the app
 *          (which references WirelessScanner unconditionally) building and running everywhere.
 * @reason A platform without a wireless backend should still launch and operate every other
 *         sensor — the wireless panel simply reports "unavailable" rather than failing the link.
 *
 * WINDOWS PORT TODO: replace this stub on Win32 with a real backend:
 *   - Wi-Fi  : Native Wifi API (wlanapi.h — WlanOpenHandle / WlanGetNetworkBssList) for AP count
 *              and best RSSI. Link wlanapi.lib.
 *   - Bluetooth-LE : WinRT Windows.Devices.Bluetooth.Advertisement
 *              (BluetoothLEAdvertisementWatcher) for BLE device count + best RSSI. Requires C++/WinRT.
 * Emit WirelessSnapshot on the same `snapshot(...)` signal at the requested cadence and the UI
 * lights up automatically. See docs/WINDOWS_PORT.md.
 *
 * CHANGELOG:
 * 2026-05-29 - Initial non-Apple stub split out for the Windows port.
 */

#include "sensors/WirelessScanner.h"

namespace bthl::spiritbox {

// The header keeps a PIMPL; we define a trivial one so unique_ptr has a complete type to delete.
struct WirelessScanner::Impl {
    bool running{false};
};

WirelessScanner::WirelessScanner(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {}

WirelessScanner::~WirelessScanner() = default;

// We accept the call but never start a timer: there is no backend on this platform yet, so we
// stay quiet rather than emitting fabricated zeros (every snapshot here would be wifiAvailable=
// false / btAvailable=false, which the UI already renders as "—").
void WirelessScanner::start(int /*intervalMs*/) {
    d->running = false;  // We report not-running so callers/UI know no data will arrive.
}

void WirelessScanner::stop() {
    d->running = false;
}

bool WirelessScanner::isRunning() const {
    return d->running;
}

} // namespace bthl::spiritbox
