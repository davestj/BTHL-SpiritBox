/**
 * @file src/sensors/WirelessScanner.h
 * @title WirelessScanner - macOS Wi-Fi + Bluetooth environmental RF presence
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We scan the local Wi-Fi and Bluetooth-LE environment as additional ambient-RF
 *          sensors: how many access points / BLE devices are nearby and the strongest signal
 *          (RSSI). This rounds out the multi-sensor picture alongside the SDR sweep and EMF.
 * @reason A sudden change in the local RF population (new BLE beacons, AP signal swings) is
 *         another environmental signal an investigator may want logged and correlated.
 *
 * Implementation is Obj-C++ (CoreWLAN + CoreBluetooth) in WirelessScanner.mm; this header is
 * pure C++/Qt so any translation unit can use it. On non-Apple platforms it is a no-op stub.
 *
 * macOS permissions required (handled by Info.plist):
 *   - NSBluetoothAlwaysUsageDescription   (CoreBluetooth scanning)
 *   - NSLocationWhenInUseUsageDescription  (CoreWLAN scan/SSID on macOS 14+)
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#ifndef BTHL_SPIRITBOX_WIRELESS_SCANNER_H
#define BTHL_SPIRITBOX_WIRELESS_SCANNER_H

#include <QObject>
#include <QString>
#include <memory>

namespace bthl::spiritbox {

/**
 * @struct WirelessSnapshot
 * @purpose One sample of the ambient wireless environment.
 */
struct WirelessSnapshot {
    int  wifiCount{0};          ///< Number of Wi-Fi access points seen this scan
    int  wifiBestRssiDbm{0};    ///< Strongest AP RSSI (dBm; 0 if none)
    bool wifiAvailable{false};  ///< Wi-Fi scanning available + permitted
    int  btCount{0};            ///< Number of distinct BLE devices seen this window
    int  btBestRssiDbm{0};      ///< Strongest BLE RSSI (dBm; 0 if none)
    bool btAvailable{false};    ///< Bluetooth powered on + permitted
};

class WirelessScanner : public QObject {
    Q_OBJECT
public:
    explicit WirelessScanner(QObject* parent = nullptr);
    ~WirelessScanner() override;

    /// We begin periodic scanning at the given cadence (default 5 s).
    void start(int intervalMs = 5000);
    void stop();
    [[nodiscard]] bool isRunning() const;

signals:
    /// We emit a fresh snapshot of the wireless environment each cycle.
    void snapshot(const WirelessSnapshot& snap);

private:
    struct Impl;
    std::unique_ptr<Impl> d;   ///< PIMPL hides the Obj-C / framework types
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_WIRELESS_SCANNER_H
