/**
 * @file src/sensors/USBDeviceManager.cpp
 * @title USBDeviceManager Implementation
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We implement USB device discovery using Qt serial port enumeration, Qt Multimedia
 *          device queries, and SoapySDR enumeration for complete hardware inventory.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation with serial, video, audio, and SDR enumeration
 */

#include "sensors/USBDeviceManager.h"
#include <QSerialPortInfo>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QJsonObject>
#include <QDebug>
#include <SoapySDR/Device.hpp>
#include <algorithm>

namespace bthl::spiritbox {

USBDeviceManager::USBDeviceManager(QObject* parent)
    : QObject(parent), m_pollTimer(new QTimer(this)) {
    connect(m_pollTimer, &QTimer::timeout, this, &USBDeviceManager::onPollTimer);
    initClassificationDb();
    qInfo() << "USBDeviceManager: We initialized with" << m_classificationDb.size() << "device classifications";
}

USBDeviceManager::~USBDeviceManager() { stopMonitoring(); }

void USBDeviceManager::startMonitoring(int pollIntervalMs) {
    m_pollTimer->setInterval(pollIntervalMs);
    m_pollTimer->start();
    onPollTimer();
    qInfo() << "USBDeviceManager: We started USB monitoring at" << pollIntervalMs << "ms intervals";
}

void USBDeviceManager::stopMonitoring() { m_pollTimer->stop(); }

QList<USBDeviceDescriptor> USBDeviceManager::scanDevices() {
    QList<USBDeviceDescriptor> allDevices;
    enumerateSerialDevices(allDevices);
    enumerateVideoDevices(allDevices);
    enumerateAudioDevices(allDevices);
    enumerateSdrDevices(allDevices);
    for (auto& device : allDevices) {
        if (device.suggestedCategory == SensorCategory::UNKNOWN)
            device.suggestedCategory = classifyDevice(device);
    }
    return allDevices;
}

QList<USBDeviceDescriptor> USBDeviceManager::connectedDevices() const { return m_knownDevices.values(); }

void USBDeviceManager::overrideDeviceCategory(const QString& busPath, SensorCategory category) {
    if (m_knownDevices.contains(busPath)) m_knownDevices[busPath].suggestedCategory = category;
}

void USBDeviceManager::addDeviceClassification(uint16_t vendorId, uint16_t productId,
                                                SensorCategory category, const QString& name) {
    m_classificationDb.append({vendorId, productId, category, name});
}

QJsonArray USBDeviceManager::devicesToJson() const {
    QJsonArray arr;
    for (const auto& d : m_knownDevices) {
        QJsonObject obj;
        obj["path"] = d.path; obj["name"] = d.name; obj["manufacturer"] = d.manufacturer;
        obj["vendor_id"] = d.vendorId; obj["product_id"] = d.productId;
        obj["serial"] = d.serial; obj["bus_path"] = d.busPath;
        obj["category"] = static_cast<int>(d.suggestedCategory);
        obj["category_name"] = sensorCategoryToString(d.suggestedCategory);
        arr.append(obj);
    }
    return arr;
}

void USBDeviceManager::onPollTimer() {
    diffDeviceList(scanDevices());
}

void USBDeviceManager::enumerateSerialDevices(QList<USBDeviceDescriptor>& devices) {
    for (const auto& port : QSerialPortInfo::availablePorts()) {
        if (port.isNull()) continue;
        USBDeviceDescriptor desc;
        desc.path = port.systemLocation();
        desc.name = port.description().isEmpty() ? port.portName() : port.description();
        desc.manufacturer = port.manufacturer();
        desc.serial = port.serialNumber();
        desc.vendorId = port.vendorIdentifier();
        desc.productId = port.productIdentifier();
        desc.busPath = QString("serial:%1").arg(port.portName());
        QString combined = (desc.name + " " + desc.manufacturer).toLower();
        if (combined.contains("gq") || combined.contains("emf"))
            desc.suggestedCategory = SensorCategory::EMF_METER;
        else if (combined.contains("thermo") || combined.contains("temperature"))
            desc.suggestedCategory = SensorCategory::THERMAL;
        devices.append(desc);
    }
}

void USBDeviceManager::enumerateVideoDevices(QList<USBDeviceDescriptor>& devices) {
    for (const auto& cam : QMediaDevices::videoInputs()) {
        USBDeviceDescriptor desc;
        desc.path = cam.id().constData();
        desc.name = cam.description();
        desc.busPath = QString("video:%1").arg(QString(cam.id()));
        desc.suggestedCategory = SensorCategory::VISUAL_CAPTURE;
        QString nameLower = desc.name.toLower();
        if (nameLower.contains("thermal") || nameLower.contains("flir") || nameLower.contains("seek"))
            desc.suggestedCategory = SensorCategory::THERMAL;
        devices.append(desc);
    }
}

void USBDeviceManager::enumerateAudioDevices(QList<USBDeviceDescriptor>& devices) {
    for (const auto& input : QMediaDevices::audioInputs()) {
        USBDeviceDescriptor desc;
        desc.path = input.id().constData();
        desc.name = input.description();
        desc.busPath = QString("audio_in:%1").arg(QString(input.id()));
        desc.suggestedCategory = SensorCategory::AUDIO_CAPTURE;
        devices.append(desc);
    }
}

void USBDeviceManager::enumerateSdrDevices(QList<USBDeviceDescriptor>& devices) {
    try {
        for (const auto& kwargs : SoapySDR::Device::enumerate()) {
            USBDeviceDescriptor desc;
            if (kwargs.count("label")) desc.name = QString::fromStdString(kwargs.at("label"));
            else if (kwargs.count("product")) desc.name = QString::fromStdString(kwargs.at("product"));
            else desc.name = "SDR Device";
            if (kwargs.count("driver")) desc.manufacturer = QString::fromStdString(kwargs.at("driver"));
            if (kwargs.count("serial")) desc.serial = QString::fromStdString(kwargs.at("serial"));
            desc.busPath = QString("sdr:%1:%2").arg(desc.manufacturer, desc.serial);
            desc.suggestedCategory = SensorCategory::RF_RECEIVER;
            devices.append(desc);
        }
    } catch (const std::exception& ex) {
        qWarning() << "USBDeviceManager: SDR enumeration error:" << ex.what();
    }
}

SensorCategory USBDeviceManager::classifyDevice(const USBDeviceDescriptor& device) const {
    for (const auto& cls : m_classificationDb)
        if (cls.vendorId == device.vendorId && cls.productId == device.productId)
            return cls.category;
    QString combined = (device.name + " " + device.manufacturer).toLower();
    if (combined.contains("sdr") || combined.contains("rtl") || combined.contains("hackrf") ||
        combined.contains("airspy")) return SensorCategory::RF_RECEIVER;
    if (combined.contains("emf") || combined.contains("gq") || combined.contains("trifield"))
        return SensorCategory::EMF_METER;
    if (combined.contains("microphone") || combined.contains("audio") || combined.contains("focusrite") ||
        combined.contains("scarlett")) return SensorCategory::AUDIO_CAPTURE;
    if (combined.contains("webcam") || combined.contains("camera")) return SensorCategory::VISUAL_CAPTURE;
    if (combined.contains("thermal") || combined.contains("flir")) return SensorCategory::THERMAL;
    if (combined.contains("accelerometer") || combined.contains("motion")) return SensorCategory::MOTION;
    if (combined.contains("magnetometer") || combined.contains("compass")) return SensorCategory::GEOMAGNETIC;
    if (combined.contains("barometer") || combined.contains("humidity") || combined.contains("geiger"))
        return SensorCategory::ENVIRONMENTAL;
    return SensorCategory::UNKNOWN;
}

void USBDeviceManager::diffDeviceList(const QList<USBDeviceDescriptor>& newDevices) {
    QMap<QString, USBDeviceDescriptor> newMap;
    for (const auto& d : newDevices) newMap[d.busPath] = d;
    QStringList disconnected;
    for (auto it = m_knownDevices.begin(); it != m_knownDevices.end(); ++it)
        if (!newMap.contains(it.key())) disconnected.append(it.key());
    bool changed = !disconnected.isEmpty();
    for (const auto& key : disconnected) {
        auto device = m_knownDevices.take(key);
        emit deviceDisconnected(device);
    }
    for (auto it = newMap.begin(); it != newMap.end(); ++it) {
        if (!m_knownDevices.contains(it.key())) {
            auto device = it.value();
            device.isNew = true;
            m_knownDevices[it.key()] = device;
            changed = true;
            emit deviceConnected(device);
            emit statusUpdated(QString("New device: %1 [%2]")
                               .arg(device.name, sensorCategoryToString(device.suggestedCategory)));
        }
    }
    if (changed) emit deviceListChanged();
}

void USBDeviceManager::initClassificationDb() {
    // We populate with known paranormal investigation equipment
    addDeviceClassification(0x0BDA, 0x2832, SensorCategory::RF_RECEIVER, "RTL-SDR (RTL2832U)");
    addDeviceClassification(0x0BDA, 0x2838, SensorCategory::RF_RECEIVER, "RTL-SDR (RTL2838)");
    addDeviceClassification(0x1D50, 0x6089, SensorCategory::RF_RECEIVER, "HackRF One");
    addDeviceClassification(0x1D50, 0x604B, SensorCategory::RF_RECEIVER, "Airspy");
    addDeviceClassification(0x1D50, 0x60A1, SensorCategory::RF_RECEIVER, "Airspy HF+");
    addDeviceClassification(0x10C4, 0xEA60, SensorCategory::EMF_METER, "GQ EMF-390 (CP210x)");
    addDeviceClassification(0x0483, 0x5740, SensorCategory::RF_RECEIVER, "TinySA/NanoVNA (STM32)");
    addDeviceClassification(0x1235, 0x8200, SensorCategory::AUDIO_CAPTURE, "Focusrite Scarlett");
    addDeviceClassification(0x1235, 0x8210, SensorCategory::AUDIO_CAPTURE, "Focusrite Scarlett 2i2");
    addDeviceClassification(0x2573, 0x0001, SensorCategory::AUDIO_CAPTURE, "Universal Audio");
    addDeviceClassification(0x09CB, 0x1996, SensorCategory::THERMAL, "FLIR ONE");
    addDeviceClassification(0x289D, 0x0010, SensorCategory::THERMAL, "Seek Thermal");
    addDeviceClassification(0x2341, 0x0043, SensorCategory::ENVIRONMENTAL, "Arduino Mega 2560");
    addDeviceClassification(0x2341, 0x0001, SensorCategory::ENVIRONMENTAL, "Arduino Uno");
}

} // namespace bthl::spiritbox
