/**
 * @file src/session/SessionRecorder.cpp
 * @title SessionRecorder Implementation
 * @author David St John (davestj)
 * @date 2026-03-31
 * @purpose We implement investigation session recording to a structured directory.
 * @reason Complete session archives enable post-investigation review, evidence sharing,
 *         and reproducibility of findings.
 *
 * CHANGELOG:
 * 2026-03-31 - Initial implementation
 */

#include "session/SessionRecorder.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <cstring>

namespace bthl::spiritbox {

SessionRecorder::SessionRecorder(QObject* parent)
    : QObject(parent) {
}

SessionRecorder::~SessionRecorder() {
    if (m_recording) stopRecording();
}

bool SessionRecorder::startRecording(const QString& sessionDir,
                                      const InvestigationSession& session) {
    m_sessionDir = sessionDir;
    QDir dir;
    if (!dir.mkpath(sessionDir)) {
        qCritical() << "SessionRecorder: We could not create session directory:" << sessionDir;
        return false;
    }
    dir.mkpath(sessionDir + "/audio_snippets");

    // We write the session metadata file
    QFile metaFile(sessionDir + "/session.json");
    if (metaFile.open(QIODevice::WriteOnly)) {
        QJsonObject meta;
        meta["session_id"] = session.sessionId.toString();
        meta["name"] = session.name;
        meta["location"] = session.location;
        meta["start_time"] = session.startTime.toString(Qt::ISODate);
        meta["connected_devices"] = session.connectedDevices;
        meta["app_version"] = "1.0.0";
        meta["platform"] = QSysInfo::prettyProductName();
        metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
        metaFile.close();
    }

    // We open the newline-delimited event log
    m_eventLog = std::make_unique<QFile>(sessionDir + "/events.jsonl");
    if (!m_eventLog->open(QIODevice::WriteOnly | QIODevice::Append)) {
        qCritical() << "SessionRecorder: We could not open event log";
        return false;
    }

    // We open the EMF CSV timeline
    m_emfCsvFile = std::make_unique<QFile>(sessionDir + "/emf_timeline.csv");
    if (m_emfCsvFile->open(QIODevice::WriteOnly)) {
        m_emfCsvFile->write("timestamp,emf_milligauss,ef_vm,rf_mw_cm2,is_spike\n");
    }

    // We open the continuous audio WAV file
    m_audioFile = std::make_unique<QFile>(sessionDir + "/audio_sweep.wav");
    if (m_audioFile->open(QIODevice::WriteOnly)) {
        writeWavHeader(*m_audioFile, m_audioSampleRate, 16);
    }

    m_anomalies = QJsonArray();
    m_transcriptions = QJsonArray();
    m_correlatedEvents = QJsonArray();
    m_audioSamplesWritten = 0;
    m_snippetCount = 0;
    m_bytesWritten = 0;
    m_recording = true;
    m_recordTimer.start();

    qInfo() << "SessionRecorder: We started recording to" << sessionDir;
    return true;
}

void SessionRecorder::stopRecording() {
    if (!m_recording) return;
    m_recording = false;

    // We finalize the WAV file with correct size headers
    if (m_audioFile && m_audioFile->isOpen()) {
        finalizeWavFile(*m_audioFile);
        m_audioFile->close();
    }

    if (m_eventLog && m_eventLog->isOpen()) m_eventLog->close();
    if (m_emfCsvFile && m_emfCsvFile->isOpen()) m_emfCsvFile->close();

    // We write accumulated JSON arrays
    auto writeJsonArray = [this](const QString& filename, const QJsonArray& array) {
        QFile file(m_sessionDir + "/" + filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
            file.close();
        }
    };

    writeJsonArray("anomalies.json", m_anomalies);
    writeJsonArray("transcriptions.json", m_transcriptions);
    writeJsonArray("correlated_events.json", m_correlatedEvents);

    qInfo() << "SessionRecorder: We finalized the session recording."
            << "Total bytes:" << m_bytesWritten
            << "Audio samples:" << m_audioSamplesWritten
            << "Snippets:" << m_snippetCount;
}

bool SessionRecorder::isRecording() const { return m_recording; }
QString SessionRecorder::sessionDirectory() const { return m_sessionDir; }
uint64_t SessionRecorder::bytesWritten() const { return m_bytesWritten; }

void SessionRecorder::onSensorReading(const SensorReading& reading) {
    if (!m_recording) return;

    QJsonObject event;
    event["type"] = "sensor_reading";
    event["sensor_id"] = reading.sensorId.toString();
    event["category"] = static_cast<int>(reading.category);
    event["timestamp"] = reading.sessionTimestamp;
    event["wall_clock"] = reading.wallClockTime.toString(Qt::ISODate);
    event["data"] = reading.data;
    event["anomaly_score"] = static_cast<double>(reading.anomalyScore);
    writeEventLine(event);
}

void SessionRecorder::onSensorAnomaly(const SensorReading& reading) {
    if (!m_recording) return;

    QJsonObject event;
    event["type"] = "anomaly";
    event["sensor_id"] = reading.sensorId.toString();
    event["category"] = static_cast<int>(reading.category);
    event["timestamp"] = reading.sessionTimestamp;
    event["anomaly_score"] = static_cast<double>(reading.anomalyScore);
    event["data"] = reading.data;
    writeEventLine(event);
}

void SessionRecorder::onMultiSensorAnomaly(const MultiSensorAnomaly& anomaly) {
    if (!m_recording) return;

    QJsonObject obj;
    obj["timestamp"] = anomaly.timestamp;
    obj["sensor_count"] = static_cast<int>(anomaly.sensorCount);
    obj["aggregate_score"] = static_cast<double>(anomaly.aggregateScore);
    obj["summary"] = anomaly.summary;

    QJsonArray readings;
    for (const auto& r : anomaly.readings) {
        QJsonObject rObj;
        rObj["sensor_id"] = r.sensorId.toString();
        rObj["category"] = static_cast<int>(r.category);
        rObj["anomaly_score"] = static_cast<double>(r.anomalyScore);
        rObj["data"] = r.data;
        readings.append(rObj);
    }
    obj["readings"] = readings;

    m_anomalies.append(obj);
}

void SessionRecorder::onAudioSamples(const std::vector<float>& samples,
                                      double /*freqHz*/, uint32_t sampleRate) {
    if (!m_recording || !m_audioFile || !m_audioFile->isOpen()) return;

    m_audioSampleRate = sampleRate;

    // We convert float samples to int16 for WAV storage
    for (float s : samples) {
        int16_t pcm = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
        m_audioFile->write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
        m_bytesWritten += sizeof(int16_t);
    }

    m_audioSamplesWritten += static_cast<uint32_t>(samples.size());
}

void SessionRecorder::onVoiceDetected(const VoiceDetectionEvent& event) {
    if (!m_recording) return;

    // We save the audio snippet as a separate WAV file
    QString snippetPath = saveAudioSnippet(event.audioSnippet, 16000, event.timestamp);

    QJsonObject obj;
    obj["type"] = "voice_detected";
    obj["timestamp"] = event.timestamp;
    obj["frequency_hz"] = event.frequencyHz;
    obj["confidence"] = static_cast<double>(event.confidence);
    obj["energy_db"] = static_cast<double>(event.energyDb);
    obj["zcr"] = static_cast<double>(event.zeroCrossingRate);
    obj["spectral_flatness"] = static_cast<double>(event.spectralFlatness);
    obj["snippet_file"] = snippetPath;
    writeEventLine(obj);
}

void SessionRecorder::onTranscriptionReady(const TranscriptionResult& result) {
    if (!m_recording) return;

    QJsonObject obj;
    obj["text"] = result.text;
    obj["timestamp"] = result.timestamp;
    obj["frequency_hz"] = result.frequencyHz;
    obj["vad_confidence"] = static_cast<double>(result.confidence);
    obj["whisper_probability"] = static_cast<double>(result.whisperProbability);
    obj["language"] = result.language;
    obj["processing_time_ms"] = static_cast<qint64>(result.processingTimeMs);
    m_transcriptions.append(obj);

    QJsonObject event;
    event["type"] = "transcription";
    event["timestamp"] = result.timestamp;
    event["text"] = result.text;
    event["frequency_hz"] = result.frequencyHz;
    writeEventLine(event);
}

void SessionRecorder::onEMFReading(const EMFReading& reading) {
    if (!m_recording || !m_emfCsvFile || !m_emfCsvFile->isOpen()) return;

    QString line = QString("%1,%2,%3,%4,%5\n")
        .arg(reading.timestamp, 0, 'f', 4)
        .arg(reading.emfMilligauss, 0, 'f', 3)
        .arg(reading.efVm, 0, 'f', 3)
        .arg(reading.rfMwCm2, 0, 'f', 6)
        .arg(reading.isSpike ? 1 : 0);

    QByteArray lineBytes = line.toUtf8();
    m_emfCsvFile->write(lineBytes);
    m_bytesWritten += static_cast<uint64_t>(lineBytes.size());
}

void SessionRecorder::onCorrelatedEvent(const CorrelatedEvent& event) {
    if (!m_recording) return;

    QJsonObject obj;
    obj["timestamp"] = event.timestamp;
    obj["emf_milligauss"] = event.emfReading.emfMilligauss;
    obj["voice_frequency_hz"] = event.voiceEvent.frequencyHz;
    obj["voice_confidence"] = static_cast<double>(event.voiceEvent.confidence);
    obj["time_delta_ms"] = event.timeDeltaMs;
    obj["correlation_score"] = static_cast<double>(event.correlationScore);
    obj["has_transcription"] = event.hasTranscription;
    if (event.hasTranscription) {
        obj["transcription_text"] = event.transcription.text;
    }
    m_correlatedEvents.append(obj);
}

// ─── WAV File Helpers ──────────────────────────────────────────────────────────

bool SessionRecorder::writeWavHeader(QFile& file, uint32_t sampleRate, uint16_t bitsPerSample) {
    // We write a standard PCM WAV header with placeholder size fields
    uint16_t channels = 1;
    uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    uint16_t blockAlign = channels * bitsPerSample / 8;

    // RIFF header
    file.write("RIFF", 4);
    uint32_t placeholder = 0;
    file.write(reinterpret_cast<const char*>(&placeholder), 4); // We update this at finalization
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&placeholder), 4); // We update this at finalization

    return true;
}

void SessionRecorder::finalizeWavFile(QFile& file) {
    // We update the RIFF and data chunk sizes
    uint32_t dataSize = m_audioSamplesWritten * 2; // 16-bit samples = 2 bytes each
    uint32_t riffSize = dataSize + 36;

    file.seek(4);
    file.write(reinterpret_cast<const char*>(&riffSize), 4);

    file.seek(40);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
}

QString SessionRecorder::saveAudioSnippet(const std::vector<float>& samples,
                                           uint32_t sampleRate, double timestamp) {
    m_snippetCount++;
    QString filename = QString("snippet_%1_%2.wav")
        .arg(m_snippetCount, 4, 10, QChar('0'))
        .arg(timestamp, 0, 'f', 2);
    QString fullPath = m_sessionDir + "/audio_snippets/" + filename;

    QFile snippetFile(fullPath);
    if (snippetFile.open(QIODevice::WriteOnly)) {
        writeWavHeader(snippetFile, sampleRate, 16);

        for (float s : samples) {
            int16_t pcm = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
            snippetFile.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
        }

        // We finalize the snippet WAV header
        uint32_t dataSize = static_cast<uint32_t>(samples.size()) * 2;
        uint32_t riffSize = dataSize + 36;
        snippetFile.seek(4);
        snippetFile.write(reinterpret_cast<const char*>(&riffSize), 4);
        snippetFile.seek(40);
        snippetFile.write(reinterpret_cast<const char*>(&dataSize), 4);
        snippetFile.close();
    }

    return "audio_snippets/" + filename;
}

void SessionRecorder::writeEventLine(const QJsonObject& event) {
    if (!m_eventLog || !m_eventLog->isOpen()) return;
    QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    m_eventLog->write(line);
    m_bytesWritten += static_cast<uint64_t>(line.size());
}

} // namespace bthl::spiritbox
