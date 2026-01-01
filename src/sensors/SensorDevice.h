/**
 * @file src/sensors/SensorDevice.h
 * @title SensorDevice - Universal Paranormal Sensor Abstraction
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We define a universal base interface for all sensor devices connected to the
 *          investigation command center. Every USB device implements this interface.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial abstraction layer with device categories and event system
 */

#ifndef BTHL_SPIRITBOX_SENSOR_DEVICE_H
#define BTHL_SPIRITBOX_SENSOR_DEVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <vector>
#include <memory>

namespace bthl::spiritbox {

enum class SensorCategory : uint8_t {
    RF_RECEIVER = 0, EMF_METER, AUDIO_CAPTURE, VISUAL_CAPTURE,
    THERMAL, ENVIRONMENTAL, MOTION, GEOMAGNETIC, CUSTOM, UNKNOWN
};

enum class SensorState : uint8_t {
    DISCONNECTED = 0, DISCOVERED, INITIALIZING, READY, CAPTURING, ERROR, SUSPENDED
};

struct SensorReading {
    QUuid sensorId;
    SensorCategory category;
    double sessionTimestamp;
    QDateTime wallClockTime;
    QJsonObject data;
    float anomalyScore;
    bool isAnomaly;
};

struct SensorDeviceInfo {
    QUuid id;
    QString name;
    QString manufacturer;
    QString model;
    QString serialNumber;
    QString connectionPath;
    SensorCategory category;
    SensorState state;
    QJsonObject capabilities;
};

class SensorDevice : public QObject {
    Q_OBJECT
public:
    explicit SensorDevice(QObject* parent = nullptr);
    ~SensorDevice() override = default;

    [[nodiscard]] QUuid id() const;
    [[nodiscard]] virtual QString deviceName() const = 0;
    [[nodiscard]] virtual SensorCategory category() const = 0;
    [[nodiscard]] virtual SensorDeviceInfo deviceInfo() const;

    virtual bool initialize() = 0;
    virtual bool startCapture() = 0;
    virtual void stopCapture() = 0;
    virtual void shutdown() = 0;

    [[nodiscard]] SensorState state() const;
    [[nodiscard]] bool isCapturing() const;

    void setAnomalyThreshold(float threshold);
    [[nodiscard]] float anomalyThreshold() const;
    virtual void setPollingRate(double hz);
    [[nodiscard]] double pollingRate() const;

    [[nodiscard]] virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);

signals:
    void readingAvailable(const SensorReading& reading);
    void anomalyDetected(const SensorReading& reading);
    void stateChanged(SensorState newState);
    void errorOccurred(const QString& message);

protected:
    void setState(SensorState newState);
    SensorReading buildReading(const QJsonObject& data, float anomalyScore);

    QUuid m_id;
    SensorState m_state{SensorState::DISCONNECTED};
    float m_anomalyThreshold{0.7f};
    double m_pollingRate{10.0};
    QDateTime m_sessionStart;
};

QString sensorCategoryToString(SensorCategory cat);
QString sensorStateToString(SensorState state);

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SENSOR_DEVICE_H
