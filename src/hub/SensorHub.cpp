/**
 * @file src/hub/SensorHub.cpp
 * @title SensorHub Implementation
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We implement the central command center orchestration layer.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation with session management and multi-sensor anomaly detection
 */

#include "hub/SensorHub.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bthl::spiritbox {

SensorHub::SensorHub(QObject* parent)
    : QObject(parent), m_usbManager(new USBDeviceManager(this)) {
    connect(m_usbManager, &USBDeviceManager::deviceConnected, this, &SensorHub::onUSBDeviceConnected);
    connect(m_usbManager, &USBDeviceManager::deviceDisconnected, this, &SensorHub::onUSBDeviceDisconnected);
    qInfo() << "SensorHub: We initialized the paranormal investigation command center";
}

SensorHub::~SensorHub() {
    if (m_investigationState == InvestigationState::RECORDING ||
        m_investigationState == InvestigationState::PAUSED) stopSession();
    stopAllSensors();
}

QUuid SensorHub::registerSensor(std::shared_ptr<SensorDevice> device) {
    QUuid id = device->id();
    connect(device.get(), &SensorDevice::readingAvailable, this, &SensorHub::onSensorReading);
    connect(device.get(), &SensorDevice::anomalyDetected, this, &SensorHub::onSensorAnomaly);
    m_sensors[id] = device;
    qInfo() << "SensorHub: We registered" << device->deviceName()
            << "(" << sensorCategoryToString(device->category()) << ")";
    emit sensorListChanged();
    return id;
}

void SensorHub::unregisterSensor(const QUuid& sensorId) {
    auto it = m_sensors.find(sensorId);
    if (it != m_sensors.end()) {
        auto device = it.value();
        device->stopCapture();
        device->shutdown();
        disconnect(device.get(), nullptr, this, nullptr);
        m_sensors.erase(it);
        emit sensorListChanged();
    }
}

std::shared_ptr<SensorDevice> SensorHub::sensor(const QUuid& sensorId) const {
    auto it = m_sensors.find(sensorId);
    return (it != m_sensors.end()) ? it.value() : nullptr;
}

QList<std::shared_ptr<SensorDevice>> SensorHub::allSensors() const { return m_sensors.values(); }

QList<std::shared_ptr<SensorDevice>> SensorHub::sensorsByCategory(SensorCategory category) const {
    QList<std::shared_ptr<SensorDevice>> result;
    for (const auto& s : m_sensors) if (s->category() == category) result.append(s);
    return result;
}

USBDeviceManager* SensorHub::usbManager() { return m_usbManager; }
int SensorHub::sensorCount() const { return m_sensors.size(); }

QUuid SensorHub::startSession(const QString& name, const QString& location) {
    if (m_investigationState == InvestigationState::RECORDING) stopSession();
    m_session = InvestigationSession{};
    m_session.sessionId = QUuid::createUuid();
    m_session.name = name;
    m_session.location = location;
    m_session.startTime = QDateTime::currentDateTimeUtc();
    m_session.connectedDevices = m_usbManager->devicesToJson();
    m_sessionTimer.start();
    m_pendingAnomalies.clear();
    m_anomalyHistory.clear();
    setInvestigationState(InvestigationState::RECORDING);
    startAllSensors();
    qInfo() << "SensorHub: We started investigation" << name << "at" << location
            << "with" << m_sensors.size() << "sensors";
    emit statusSummary(QString("Investigation '%1' started with %2 sensor(s)")
                       .arg(name).arg(m_sensors.size()));
    return m_session.sessionId;
}

void SensorHub::pauseSession() {
    if (m_investigationState != InvestigationState::RECORDING) return;
    stopAllSensors();
    setInvestigationState(InvestigationState::PAUSED);
}

void SensorHub::resumeSession() {
    if (m_investigationState != InvestigationState::PAUSED) return;
    startAllSensors();
    setInvestigationState(InvestigationState::RECORDING);
}

void SensorHub::stopSession() {
    stopAllSensors();
    m_session.endTime = QDateTime::currentDateTimeUtc();
    setInvestigationState(InvestigationState::IDLE);
    qInfo() << "SensorHub: Session complete. Readings:" << m_session.totalReadings
            << "Anomalies:" << m_session.totalAnomalies
            << "Correlated:" << m_session.correlatedEvents;
    emit statusSummary(QString("Investigation complete. %1 readings, %2 anomalies, %3 correlations")
                       .arg(m_session.totalReadings).arg(m_session.totalAnomalies)
                       .arg(m_session.correlatedEvents));
}

InvestigationState SensorHub::investigationState() const { return m_investigationState; }
InvestigationSession SensorHub::currentSession() const { return m_session; }
double SensorHub::sessionElapsedSeconds() const {
    return m_sessionTimer.isValid() ? m_sessionTimer.elapsed() / 1000.0 : 0.0;
}

void SensorHub::setCorrelationWindowMs(uint32_t windowMs) { m_correlationWindowMs = windowMs; }
void SensorHub::setMinCorrelatedSensors(uint32_t count) { m_minCorrelatedSensors = std::max(2u, count); }
const std::vector<MultiSensorAnomaly>& SensorHub::anomalyHistory() const { return m_anomalyHistory; }

void SensorHub::startAllSensors() {
    for (auto& s : m_sensors)
        if (s->state() == SensorState::READY || s->state() == SensorState::SUSPENDED)
            s->startCapture();
}

void SensorHub::stopAllSensors() {
    for (auto& s : m_sensors) if (s->isCapturing()) s->stopCapture();
}

void SensorHub::setGlobalAnomalyThreshold(float threshold) {
    for (auto& s : m_sensors) s->setAnomalyThreshold(threshold);
}

void SensorHub::onSensorReading(const SensorReading& reading) {
    m_session.totalReadings++;
    emit sensorReadingReceived(reading);
}

void SensorHub::onSensorAnomaly(const SensorReading& reading) {
    m_session.totalAnomalies++;
    m_pendingAnomalies.push_back(reading);
    while (m_pendingAnomalies.size() > 200) m_pendingAnomalies.pop_front();
    emit sensorAnomalyDetected(reading);
    checkMultiSensorCorrelation();
}

void SensorHub::onUSBDeviceConnected(const USBDeviceDescriptor& device) {
    qInfo() << "SensorHub: New USB device:" << device.name
            << "(" << sensorCategoryToString(device.suggestedCategory) << ")";
    emit statusSummary(QString("New device: %1 [%2]")
                       .arg(device.name, sensorCategoryToString(device.suggestedCategory)));
}

void SensorHub::onUSBDeviceDisconnected(const USBDeviceDescriptor& device) {
    qInfo() << "SensorHub: Lost USB device:" << device.name;
    emit statusSummary(QString("Device disconnected: %1").arg(device.name));
}

void SensorHub::checkMultiSensorCorrelation() {
    purgeExpiredAnomalies();
    if (m_pendingAnomalies.size() < m_minCorrelatedSensors) return;
    double windowSec = static_cast<double>(m_correlationWindowMs) / 1000.0;

    std::vector<std::vector<SensorReading>> clusters;
    for (size_t i = 0; i < m_pendingAnomalies.size(); ++i) {
        const auto& reading = m_pendingAnomalies[i];
        bool added = false;
        for (auto& cluster : clusters) {
            double delta = std::fabs(reading.sessionTimestamp - cluster.front().sessionTimestamp);
            if (delta <= windowSec) {
                bool present = false;
                for (const auto& existing : cluster)
                    if (existing.sensorId == reading.sensorId) { present = true; break; }
                if (!present) { cluster.push_back(reading); added = true; break; }
            }
        }
        if (!added) clusters.push_back({reading});
    }

    for (const auto& cluster : clusters) {
        if (cluster.size() >= m_minCorrelatedSensors) {
            MultiSensorAnomaly anomaly;
            anomaly.readings = cluster;
            anomaly.sensorCount = static_cast<uint32_t>(cluster.size());
            double timeSum = 0.0; float scoreSum = 0.0f;
            for (const auto& r : cluster) { timeSum += r.sessionTimestamp; scoreSum += r.anomalyScore; }
            anomaly.timestamp = timeSum / cluster.size();
            anomaly.aggregateScore = scoreSum / static_cast<float>(cluster.size());
            anomaly.summary = generateAnomalySummary(anomaly);
            m_anomalyHistory.push_back(anomaly);
            m_session.correlatedEvents++;
            qInfo() << "SensorHub: MULTI-SENSOR ANOMALY #" << m_session.correlatedEvents
                    << ":" << anomaly.summary;
            emit multiSensorAnomalyDetected(anomaly);
            for (const auto& consumed : cluster) {
                auto it = std::find_if(m_pendingAnomalies.begin(), m_pendingAnomalies.end(),
                    [&consumed](const SensorReading& r) {
                        return r.sensorId == consumed.sensorId &&
                               r.sessionTimestamp == consumed.sessionTimestamp;
                    });
                if (it != m_pendingAnomalies.end()) m_pendingAnomalies.erase(it);
            }
        }
    }
}

void SensorHub::purgeExpiredAnomalies() {
    double maxAge = static_cast<double>(m_correlationWindowMs) / 1000.0 * 3.0;
    double now = sessionElapsedSeconds();
    while (!m_pendingAnomalies.empty() && (now - m_pendingAnomalies.front().sessionTimestamp) > maxAge)
        m_pendingAnomalies.pop_front();
}

QString SensorHub::generateAnomalySummary(const MultiSensorAnomaly& anomaly) const {
    QStringList names;
    for (const auto& r : anomaly.readings) {
        auto s = sensor(r.sensorId);
        names.append(s ? QString("%1(%2)").arg(s->deviceName(), sensorCategoryToString(s->category()))
                       : sensorCategoryToString(r.category));
    }
    return QString("%1 sensors: %2 (score: %3)")
        .arg(anomaly.sensorCount).arg(names.join(", ")).arg(anomaly.aggregateScore, 0, 'f', 3);
}

void SensorHub::setInvestigationState(InvestigationState newState) {
    if (m_investigationState != newState) {
        m_investigationState = newState;
        m_session.state = newState;
        emit investigationStateChanged(newState);
    }
}

} // namespace bthl::spiritbox
