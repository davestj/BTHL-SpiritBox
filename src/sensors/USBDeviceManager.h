/**
 * @file src/sensors/USBDeviceManager.h
 * @title USBDeviceManager - USB Device Discovery and Auto-Classification
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We monitor the USB bus for connected devices and automatically classify them
 *          into sensor categories. Plugging in any supported device immediately integrates
 *          it into the investigation command center.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation with polling-based USB enumeration
 */

#ifndef BTHL_SPIRITBOX_USB_DEVICE_MANAGER_H
#define BTHL_SPIRITBOX_USB_DEVICE_MANAGER_H

#include "sensors/SensorDevice.h"
#include <QObject>
#include <QTimer>
#include <QMap>
#include <QString>
#include <QJsonArray>

namespace bthl::spiritbox {

struct USBDeviceDescriptor {
    QString path;
    QString name;
    QString manufacturer;
    uint16_t vendorId{0};
    uint16_t productId{0};
    QString serial;
    QString busPath;
    SensorCategory suggestedCategory{SensorCategory::UNKNOWN};
    bool isNew{false};
};

class USBDeviceManager : public QObject {
    Q_OBJECT
public:
    explicit USBDeviceManager(QObject* parent = nullptr);
    ~USBDeviceManager() override;

    void startMonitoring(int pollIntervalMs = 2000);
    void stopMonitoring();
    QList<USBDeviceDescriptor> scanDevices();
    [[nodiscard]] QList<USBDeviceDescriptor> connectedDevices() const;
    void overrideDeviceCategory(const QString& busPath, SensorCategory category);
    void addDeviceClassification(uint16_t vendorId, uint16_t productId,
                                  SensorCategory category, const QString& name);
    [[nodiscard]] QJsonArray devicesToJson() const;

signals:
    void deviceConnected(const USBDeviceDescriptor& device);
    void deviceDisconnected(const USBDeviceDescriptor& device);
    void deviceListChanged();
    void statusUpdated(const QString& message);

private slots:
    void onPollTimer();

private:
    void enumerateSerialDevices(QList<USBDeviceDescriptor>& devices);
    void enumerateVideoDevices(QList<USBDeviceDescriptor>& devices);
    void enumerateAudioDevices(QList<USBDeviceDescriptor>& devices);
    void enumerateSdrDevices(QList<USBDeviceDescriptor>& devices);
    SensorCategory classifyDevice(const USBDeviceDescriptor& device) const;
    void diffDeviceList(const QList<USBDeviceDescriptor>& newDevices);
    void initClassificationDb();

    QTimer* m_pollTimer{nullptr};
    QMap<QString, USBDeviceDescriptor> m_knownDevices;

    struct DeviceClassification {
        uint16_t vendorId;
        uint16_t productId;
        SensorCategory category;
        QString name;
    };
    QList<DeviceClassification> m_classificationDb;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_USB_DEVICE_MANAGER_H
