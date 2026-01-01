/**
 * @file src/sensors/SensorDevice.cpp
 * @title SensorDevice Base Implementation
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We implement the shared functionality of all sensor devices.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation
 */

#include "sensors/SensorDevice.h"
#include <QDebug>

namespace bthl::spiritbox {

SensorDevice::SensorDevice(QObject* parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_sessionStart(QDateTime::currentDateTimeUtc()) {}

QUuid SensorDevice::id() const { return m_id; }

SensorDeviceInfo SensorDevice::deviceInfo() const {
    SensorDeviceInfo info;
    info.id = m_id;
    info.name = deviceName();
    info.category = category();
    info.state = m_state;
    return info;
}

SensorState SensorDevice::state() const { return m_state; }
bool SensorDevice::isCapturing() const { return m_state == SensorState::CAPTURING; }

void SensorDevice::setAnomalyThreshold(float threshold) {
    m_anomalyThreshold = std::clamp(threshold, 0.0f, 1.0f);
}
float SensorDevice::anomalyThreshold() const { return m_anomalyThreshold; }

void SensorDevice::setPollingRate(double hz) { m_pollingRate = std::max(0.1, hz); }
double SensorDevice::pollingRate() const { return m_pollingRate; }

QJsonObject SensorDevice::toJson() const {
    QJsonObject obj;
    obj["id"] = m_id.toString();
    obj["name"] = deviceName();
    obj["category"] = static_cast<int>(category());
    obj["anomaly_threshold"] = static_cast<double>(m_anomalyThreshold);
    obj["polling_rate"] = m_pollingRate;
    return obj;
}

void SensorDevice::fromJson(const QJsonObject& json) {
    m_anomalyThreshold = static_cast<float>(json["anomaly_threshold"].toDouble(0.7));
    m_pollingRate = json["polling_rate"].toDouble(10.0);
}

void SensorDevice::setState(SensorState newState) {
    if (m_state != newState) {
        m_state = newState;
        qInfo() << "SensorDevice:" << deviceName() << "state ->"
                << sensorStateToString(newState);
        emit stateChanged(newState);
    }
}

SensorReading SensorDevice::buildReading(const QJsonObject& data, float anomalyScore) {
    SensorReading reading;
    reading.sensorId = m_id;
    reading.category = category();
    reading.wallClockTime = QDateTime::currentDateTimeUtc();
    reading.sessionTimestamp = m_sessionStart.msecsTo(reading.wallClockTime) / 1000.0;
    reading.data = data;
    reading.anomalyScore = anomalyScore;
    reading.isAnomaly = anomalyScore >= m_anomalyThreshold;
    emit readingAvailable(reading);
    if (reading.isAnomaly) emit anomalyDetected(reading);
    return reading;
}

QString sensorCategoryToString(SensorCategory cat) {
    switch (cat) {
        case SensorCategory::RF_RECEIVER:    return "RF Receiver";
        case SensorCategory::EMF_METER:      return "EMF Meter";
        case SensorCategory::AUDIO_CAPTURE:  return "Audio Capture";
        case SensorCategory::VISUAL_CAPTURE: return "Visual Capture";
        case SensorCategory::THERMAL:        return "Thermal";
        case SensorCategory::ENVIRONMENTAL:  return "Environmental";
        case SensorCategory::MOTION:         return "Motion";
        case SensorCategory::GEOMAGNETIC:    return "Geomagnetic";
        case SensorCategory::CUSTOM:         return "Custom";
        case SensorCategory::UNKNOWN:        return "Unknown";
    }
    return "Unknown";
}

QString sensorStateToString(SensorState state) {
    switch (state) {
        case SensorState::DISCONNECTED:  return "Disconnected";
        case SensorState::DISCOVERED:    return "Discovered";
        case SensorState::INITIALIZING:  return "Initializing";
        case SensorState::READY:         return "Ready";
        case SensorState::CAPTURING:     return "Capturing";
        case SensorState::ERROR:         return "Error";
        case SensorState::SUSPENDED:     return "Suspended";
    }
    return "Unknown";
}

} // namespace bthl::spiritbox
