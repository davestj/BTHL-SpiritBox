/**
 * @file src/hub/SensorHub.h
 * @title SensorHub - Paranormal Investigation Command Center Core
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We serve as the central nervous system of the investigation command center.
 *          The SensorHub receives data from all connected SensorDevices, routes readings
 *          to the correlation engine, manages investigation sessions, and provides a
 *          unified API for the UI to interact with the entire sensor network.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation with session management and multi-sensor correlation
 */

#ifndef BTHL_SPIRITBOX_SENSOR_HUB_H
#define BTHL_SPIRITBOX_SENSOR_HUB_H

#include "sensors/SensorDevice.h"
#include "sensors/USBDeviceManager.h"
#include <QObject>
#include <QMap>
#include <QUuid>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include <deque>

namespace bthl::spiritbox {

enum class InvestigationState : uint8_t {
    IDLE = 0, CONFIGURING, RECORDING, PAUSED, REVIEWING
};

struct InvestigationSession {
    QUuid sessionId;
    QString name;
    QString location;
    QString notes;
    QDateTime startTime;
    QDateTime endTime;
    InvestigationState state{InvestigationState::IDLE};
    uint32_t totalReadings{0};
    uint32_t totalAnomalies{0};
    uint32_t correlatedEvents{0};
    QJsonArray connectedDevices;
};

struct MultiSensorAnomaly {
    double timestamp;
    std::vector<SensorReading> readings;
    float aggregateScore;
    uint32_t sensorCount;
    QString summary;
};

class SensorHub : public QObject {
    Q_OBJECT
public:
    explicit SensorHub(QObject* parent = nullptr);
    ~SensorHub() override;

    QUuid registerSensor(std::shared_ptr<SensorDevice> device);
    void unregisterSensor(const QUuid& sensorId);
    std::shared_ptr<SensorDevice> sensor(const QUuid& sensorId) const;
    QList<std::shared_ptr<SensorDevice>> allSensors() const;
    QList<std::shared_ptr<SensorDevice>> sensorsByCategory(SensorCategory category) const;
    USBDeviceManager* usbManager();
    [[nodiscard]] int sensorCount() const;

    QUuid startSession(const QString& name, const QString& location);
    void pauseSession();
    void resumeSession();
    void stopSession();
    [[nodiscard]] InvestigationState investigationState() const;
    [[nodiscard]] InvestigationSession currentSession() const;
    [[nodiscard]] double sessionElapsedSeconds() const;

    void setCorrelationWindowMs(uint32_t windowMs);
    void setMinCorrelatedSensors(uint32_t count);
    [[nodiscard]] const std::vector<MultiSensorAnomaly>& anomalyHistory() const;

    void startAllSensors();
    void stopAllSensors();
    void setGlobalAnomalyThreshold(float threshold);

signals:
    void sensorReadingReceived(const SensorReading& reading);
    void sensorAnomalyDetected(const SensorReading& reading);
    void multiSensorAnomalyDetected(const MultiSensorAnomaly& anomaly);
    void sensorListChanged();
    void investigationStateChanged(InvestigationState newState);
    void statusSummary(const QString& summary);

private slots:
    void onSensorReading(const SensorReading& reading);
    void onSensorAnomaly(const SensorReading& reading);
    void onUSBDeviceConnected(const USBDeviceDescriptor& device);
    void onUSBDeviceDisconnected(const USBDeviceDescriptor& device);

private:
    void checkMultiSensorCorrelation();
    void purgeExpiredAnomalies();
    QString generateAnomalySummary(const MultiSensorAnomaly& anomaly) const;
    void setInvestigationState(InvestigationState newState);

    QMap<QUuid, std::shared_ptr<SensorDevice>> m_sensors;
    USBDeviceManager* m_usbManager{nullptr};
    InvestigationSession m_session;
    QElapsedTimer m_sessionTimer;
    std::deque<SensorReading> m_pendingAnomalies;
    std::vector<MultiSensorAnomaly> m_anomalyHistory;
    uint32_t m_correlationWindowMs{1000};
    uint32_t m_minCorrelatedSensors{2};
    InvestigationState m_investigationState{InvestigationState::IDLE};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SENSOR_HUB_H
