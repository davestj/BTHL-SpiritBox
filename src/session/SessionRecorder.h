/**
 * @file src/session/SessionRecorder.h
 * @title SessionRecorder - Investigation Session Data Archiver
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We record all sensor data, audio streams, detection events, correlated anomalies,
 *          and transcription results to disk during an investigation session.
 * @reason Paranormal evidence requires chain-of-custody record keeping. Every data point
 *         must be timestamped, attributed to a sensor, and stored in a reproducible format.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation with JSON event log and WAV audio recording
 */

#ifndef BTHL_SPIRITBOX_SESSION_RECORDER_H
#define BTHL_SPIRITBOX_SESSION_RECORDER_H

#include "sensors/SensorDevice.h"
#include "hub/SensorHub.h"
#include "detection/VoiceActivityDetector.h"
#include "detection/WhisperTranscriber.h"
#include "emf/EMFCorrelator.h"
#include <QObject>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QElapsedTimer>
#include <memory>

namespace bthl::spiritbox {

class SessionRecorder : public QObject {
    Q_OBJECT

public:
    explicit SessionRecorder(QObject* parent = nullptr);
    ~SessionRecorder() override;

    bool startRecording(const QString& sessionDir, const InvestigationSession& session);
    void stopRecording();
    [[nodiscard]] bool isRecording() const;
    [[nodiscard]] QString sessionDirectory() const;
    [[nodiscard]] uint64_t bytesWritten() const;

public slots:
    void onSensorReading(const SensorReading& reading);
    void onSensorAnomaly(const SensorReading& reading);
    void onMultiSensorAnomaly(const MultiSensorAnomaly& anomaly);
    void onAudioSamples(const std::vector<float>& samples, double freqHz, uint32_t sampleRate);
    void onVoiceDetected(const VoiceDetectionEvent& event);
    void onTranscriptionReady(const TranscriptionResult& result);
    void onEMFReading(const EMFReading& reading);
    void onCorrelatedEvent(const CorrelatedEvent& event);

private:
    bool writeWavHeader(QFile& file, uint32_t sampleRate, uint16_t bitsPerSample);
    void finalizeWavFile(QFile& file);
    QString saveAudioSnippet(const std::vector<float>& samples, uint32_t sampleRate, double timestamp);
    void writeEventLine(const QJsonObject& event);

    QString m_sessionDir;
    bool m_recording{false};
    uint64_t m_bytesWritten{0};

    std::unique_ptr<QFile> m_eventLog;
    std::unique_ptr<QFile> m_audioFile;
    std::unique_ptr<QFile> m_emfCsvFile;
    QJsonArray m_anomalies;
    QJsonArray m_transcriptions;
    QJsonArray m_correlatedEvents;

    uint32_t m_audioSampleRate{16000};
    uint32_t m_audioSamplesWritten{0};
    uint32_t m_snippetCount{0};
    QElapsedTimer m_recordTimer;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SESSION_RECORDER_H
