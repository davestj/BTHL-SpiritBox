/**
 * @file src/sensors/WirelessScanner.mm
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose Obj-C++ implementation of the macOS Wi-Fi (CoreWLAN) + Bluetooth-LE (CoreBluetooth)
 *          environmental scanner. We sample nearby AP / BLE device counts and strongest RSSI.
 *
 * We run the (blocking) Wi-Fi scan on a background GCD queue so the UI never stalls, and let
 * CoreBluetooth deliver BLE discoveries on its own queue; both fold into a snapshot emitted each
 * cycle. Signals cross threads via Qt queued connections (the receiver lives on the GUI thread).
 */

#include "sensors/WirelessScanner.h"

#include <QTimer>
#include <QDebug>
#include <mutex>
#include <unordered_map>
#include <string>

#import <Foundation/Foundation.h>
#import <CoreWLAN/CoreWLAN.h>
#import <CoreBluetooth/CoreBluetooth.h>

namespace bthl::spiritbox { class WirelessScanner; }

// ─── CoreBluetooth delegate: accumulates BLE discoveries (uuid → best RSSI) ──────────
@interface BthlBleDelegate : NSObject <CBCentralManagerDelegate>
@property(nonatomic, assign) BOOL poweredOn;
- (void)drainInto:(std::unordered_map<std::string,int>*)out;
@end

@implementation BthlBleDelegate {
    std::mutex _mtx;
    std::unordered_map<std::string,int> _seen;  // peripheral UUID → strongest RSSI (dBm)
}
- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
    self.poweredOn = (central.state == CBManagerStatePoweredOn);
    if (self.poweredOn) {
        // Allow duplicates so RSSI refreshes; scan for everything.
        [central scanForPeripheralsWithServices:nil
            options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @YES}];
    }
}
- (void)centralManager:(CBCentralManager*)central
        didDiscoverPeripheral:(CBPeripheral*)peripheral
        advertisementData:(NSDictionary<NSString*,id>*)adv
        RSSI:(NSNumber*)rssi {
    (void)central; (void)adv;
    const std::string uuid = peripheral.identifier.UUIDString.UTF8String;
    const int r = rssi.intValue;
    std::lock_guard<std::mutex> lk(_mtx);
    auto it = _seen.find(uuid);
    if (it == _seen.end() || r > it->second) _seen[uuid] = r;  // keep strongest
}
- (void)drainInto:(std::unordered_map<std::string,int>*)out {
    std::lock_guard<std::mutex> lk(_mtx);
    *out = _seen;
    _seen.clear();  // each snapshot reflects the devices seen in the last window
}
@end

namespace bthl::spiritbox {

struct WirelessScanner::Impl {
    QTimer timer;
    CWWiFiClient* wifi = nil;
    CBCentralManager* central = nil;
    BthlBleDelegate* bleDelegate = nil;
    dispatch_queue_t bleQueue = nullptr;
    bool running = false;
};

WirelessScanner::WirelessScanner(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {}

WirelessScanner::~WirelessScanner() { stop(); }

bool WirelessScanner::isRunning() const { return d->running; }

void WirelessScanner::start(int intervalMs) {
    if (d->running) return;
    @autoreleasepool {
        d->wifi = [CWWiFiClient sharedWiFiClient];
        d->bleDelegate = [[BthlBleDelegate alloc] init];
        d->bleQueue = dispatch_queue_create("com.beyondthehorizonlabs.spiritbox.ble", nullptr);
        // Creating the central manager triggers the permission prompt + powers up the radio;
        // scanning starts from the delegate once state is PoweredOn.
        d->central = [[CBCentralManager alloc] initWithDelegate:d->bleDelegate
                                                          queue:d->bleQueue];
    }
    connect(&d->timer, &QTimer::timeout, this, [this]() {
        // Wi-Fi scan blocks briefly → run it off the GUI thread, then emit a snapshot.
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            WirelessSnapshot snap;
            @autoreleasepool {
                CWInterface* iface = d->wifi.interface;
                if (iface) {
                    NSError* err = nil;
                    NSSet<CWNetwork*>* nets = [iface scanForNetworksWithName:nil error:&err];
                    if (nets && !err) {
                        snap.wifiAvailable = true;
                        snap.wifiCount = (int)nets.count;
                        int best = -200;
                        for (CWNetwork* n in nets) best = MAX(best, (int)n.rssiValue);
                        snap.wifiBestRssiDbm = (snap.wifiCount > 0) ? best : 0;
                    }
                }
                snap.btAvailable = d->bleDelegate.poweredOn;
                if (snap.btAvailable) {
                    std::unordered_map<std::string,int> seen;
                    [d->bleDelegate drainInto:&seen];
                    snap.btCount = (int)seen.size();
                    int best = -200;
                    for (auto& kv : seen) best = MAX(best, kv.second);
                    snap.btBestRssiDbm = (snap.btCount > 0) ? best : 0;
                }
            }
            // Emit via the object's thread (queued) so listeners run on the GUI thread.
            QMetaObject::invokeMethod(this, [this, snap]() { emit snapshot(snap); },
                                      Qt::QueuedConnection);
        });
    });
    d->timer.start(intervalMs);
    d->running = true;
    qInfo() << "WirelessScanner: started (Wi-Fi + BLE) at" << intervalMs << "ms";
}

void WirelessScanner::stop() {
    if (!d->running) return;
    d->timer.stop();
    @autoreleasepool {
        if (d->central) { [d->central stopScan]; }
    }
    d->central = nil;
    d->bleDelegate = nil;
    d->wifi = nil;
    d->running = false;
    qInfo() << "WirelessScanner: stopped";
}

} // namespace bthl::spiritbox
