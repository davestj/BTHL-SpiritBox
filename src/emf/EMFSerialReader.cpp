/**
 * @file src/emf/EMFSerialReader.cpp
 * @title EMFSerialReader Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement the GQ EMF-390 serial communication protocol for real-time EMF data.
 * @reason The GQ EMF-390 uses a documented serial protocol that we can poll for continuous
 *         readings. We use Qt SerialPort for cross-platform serial communication.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with GQ protocol command set
 */

#include "emf/EMFSerialReader.h"
#include <QDebug>
#include <numeric>
#include <algorithm>

namespace bthl::spiritbox {

EMFSerialReader::EMFSerialReader(QObject* parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_pollTimer(new QTimer(this)) {

    connect(m_serialPort, &QSerialPort::readyRead, this, &EMFSerialReader::onDataReady);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &EMFSerialReader::onSerialError);
    connect(m_pollTimer, &QTimer::timeout, this, &EMFSerialReader::onPollTimer);

    m_baselineWindow.reserve(BASELINE_WINDOW_SIZE);
    m_pollTimer->setInterval(100); // 10 readings per second default

    qInfo() << "EMFSerialReader: We initialized the EMF serial reader";
}

EMFSerialReader::~EMFSerialReader() {
    disconnectDevice();
}

QList<QSerialPortInfo> EMFSerialReader::enumerateDevices() {
    QList<QSerialPortInfo> gqDevices;

    const auto allPorts = QSerialPortInfo::availablePorts();
    for (const auto& port : allPorts) {
        // We look for GQ Electronics vendor ID or known product descriptions
        // GQ EMF-390 typically shows as a USB CDC ACM device
        QString desc = port.description().toLower();
        QString mfg = port.manufacturer().toLower();

        if (desc.contains("gq") || mfg.contains("gq") ||
            desc.contains("emf") || desc.contains("ch340") ||
            desc.contains("usb serial") ||
            port.vendorIdentifier() == 0x1A86 || // CH340 (GQ EMF-390v2)
            port.vendorIdentifier() == 0x10C4) { // Silicon Labs CP210x
            gqDevices.append(port);
            qInfo() << "EMFSerialReader: We found potential GQ device on" << port.portName()
                    << "(" << port.description() << ")";
        }
    }

    // We also include all ports as fallback if no GQ-specific devices found
    if (gqDevices.isEmpty()) {
        qInfo() << "EMFSerialReader: We found no GQ-specific devices, listing all serial ports";
        return allPorts;
    }

    return gqDevices;
}

bool EMFSerialReader::connectDevice(const QString& portName, int baudRate) {
    if (m_serialPort->isOpen()) {
        disconnectDevice();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("We could not open serial port %1: %2")
                           .arg(portName, m_serialPort->errorString()));
        return false;
    }

    m_sessionTimer.start();
    m_baseline = 0.0;
    m_baselineWindow.clear();

    // We send a version request to verify the device is a GQ EMF meter
    sendCommand("<GETVER>>");

    emit connectionStateChanged(true);
    qInfo() << "EMFSerialReader: We connected to" << portName << "at" << baudRate << "baud";
    return true;
}

void EMFSerialReader::disconnectDevice() {
    stopReading();

    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        qInfo() << "EMFSerialReader: We disconnected from the device";
    }

    emit connectionStateChanged(false);
}

bool EMFSerialReader::isConnected() const {
    return m_serialPort->isOpen();
}

void EMFSerialReader::setPollingInterval(int intervalMs) {
    m_pollTimer->setInterval(intervalMs);
}

void EMFSerialReader::startReading() {
    if (!m_serialPort->isOpen()) {
        emit errorOccurred("We cannot start reading without a connected device");
        return;
    }

    m_reading = true;
    m_pollTimer->start();
    qInfo() << "EMFSerialReader: We started continuous EMF reading at"
            << m_pollTimer->interval() << "ms intervals";
}

void EMFSerialReader::stopReading() {
    m_reading = false;
    m_pollTimer->stop();
}

void EMFSerialReader::setSpikeThreshold(double threshold) {
    m_spikeThreshold = threshold;
}

double EMFSerialReader::spikeThreshold() const {
    return m_spikeThreshold;
}

EMFReading EMFSerialReader::lastReading() const {
    return m_lastReading;
}

double EMFSerialReader::baselineMilligauss() const {
    return m_baseline;
}

void EMFSerialReader::onDataReady() {
    m_readBuffer.append(m_serialPort->readAll());
    parseResponse(m_readBuffer);
}

void EMFSerialReader::onPollTimer() {
    if (!m_serialPort->isOpen()) return;

    // We request the current EMF reading from the GQ EMF-390
    // The <GETEMF>> command returns the current EMF field strength
    sendCommand("<GETEMF>>");
}

void EMFSerialReader::onSerialError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;

    emit errorOccurred(QString("We encountered a serial port error: %1")
                       .arg(m_serialPort->errorString()));

    if (error == QSerialPort::ResourceError) {
        // We lost the device connection
        disconnectDevice();
    }
}

void EMFSerialReader::parseResponse(const QByteArray& data) {
    // We parse the GQ EMF-390 response format
    // The device returns EMF values as floating point strings or binary data
    // depending on the firmware version and command used

    if (data.size() < 4) return; // We need at least 4 bytes for a valid reading

    // We attempt to parse as a binary float (4 bytes, big-endian)
    // The GQ EMF-390 returns EMF readings as 4-byte big-endian IEEE 754 floats
    // after the <GETEMF>> command

    if (data.size() >= 4) {
        // We extract the float value from the first 4 bytes
        uint32_t rawValue = 0;
        rawValue |= static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24;
        rawValue |= static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16;
        rawValue |= static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8;
        rawValue |= static_cast<uint32_t>(static_cast<uint8_t>(data[3]));

        float emfValue;
        std::memcpy(&emfValue, &rawValue, sizeof(float));

        // We validate the reading is within reasonable bounds
        if (std::isfinite(emfValue) && emfValue >= 0.0f && emfValue < 2000.0f) {
            EMFReading reading;
            reading.timestamp = m_sessionTimer.elapsed() / 1000.0;
            reading.emfMilligauss = static_cast<double>(emfValue);
            reading.efVm = 0.0;
            reading.rfMwCm2 = 0.0;

            // We check if this reading qualifies as a spike
            double deviation = reading.emfMilligauss - m_baseline;
            reading.isSpike = (deviation > m_spikeThreshold) ||
                              (reading.emfMilligauss > m_spikeThreshold);

            // We update our rolling baseline
            updateBaseline(reading.emfMilligauss);

            m_lastReading = reading;
            emit readingReceived(reading);

            if (reading.isSpike) {
                qInfo() << "EMFSerialReader: We detected an EMF spike:"
                        << reading.emfMilligauss << "mG (baseline:" << m_baseline << "mG)";
                emit spikeDetected(reading);
            }
        }

        // We clear the processed bytes from our buffer
        m_readBuffer = m_readBuffer.mid(4);
    }
}

void EMFSerialReader::sendCommand(const QByteArray& command) {
    if (!m_serialPort->isOpen()) return;
    m_serialPort->write(command);
    m_serialPort->flush();
}

void EMFSerialReader::updateBaseline(double emfValue) {
    m_baselineWindow.push_back(emfValue);

    // We maintain a sliding window for the baseline average
    if (m_baselineWindow.size() > BASELINE_WINDOW_SIZE) {
        m_baselineWindow.erase(m_baselineWindow.begin());
    }

    // We calculate the rolling average as our baseline
    m_baseline = std::accumulate(m_baselineWindow.begin(), m_baselineWindow.end(), 0.0)
                 / static_cast<double>(m_baselineWindow.size());
}

} // namespace bthl::spiritbox
