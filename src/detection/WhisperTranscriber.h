/**
 * @file src/detection/WhisperTranscriber.h
 * @title WhisperTranscriber - Local AI Speech-to-Text Engine
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We integrate whisper.cpp to perform local, offline speech-to-text transcription
 *          on audio snippets flagged by the VoiceActivityDetector.
 * @reason Running Whisper locally ensures zero cloud dependency, zero latency from network
 *         round-trips, and full privacy for field investigation recordings.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with async transcription via QtConcurrent
 */

#ifndef BTHL_SPIRITBOX_WHISPER_TRANSCRIBER_H
#define BTHL_SPIRITBOX_WHISPER_TRANSCRIBER_H

#include "detection/VoiceActivityDetector.h"
#include <QObject>
#include <QString>
#include <QMutex>
#include <vector>
#include <atomic>
#include <memory>

#if WHISPER_AVAILABLE
#include "whisper.h"
#endif

namespace bthl::spiritbox {

/**
 * @struct TranscriptionResult
 * @purpose We store the result of a Whisper transcription along with source metadata
 */
struct TranscriptionResult {
    QString text;               ///< The transcribed text
    double timestamp;           ///< When the voice was detected (session time)
    double frequencyHz;         ///< RF frequency source
    float confidence;           ///< VAD confidence that triggered transcription
    float whisperProbability;   ///< Whisper's own probability estimate
    QString language;           ///< Detected language
    int64_t processingTimeMs;   ///< How long transcription took
    VoiceSource source{VoiceSource::RadioSweep}; ///< Radio-band response vs investigator microphone
};

/**
 * @class WhisperTranscriber
 * @purpose We manage the whisper.cpp model lifecycle and perform async transcription
 *          of audio snippets received from the VoiceActivityDetector.
 */
class WhisperTranscriber : public QObject {
    Q_OBJECT

public:
    explicit WhisperTranscriber(QObject* parent = nullptr);
    ~WhisperTranscriber() override;

    /**
     * @brief We load a Whisper model from disk
     * @param modelPath Path to the .bin model file (e.g., ggml-base.en.bin)
     * @return true if we successfully loaded the model
     */
    bool loadModel(const QString& modelPath);

    /**
     * @brief We unload the current model and free resources
     */
    void unloadModel();

    /**
     * @brief We check if a model is currently loaded and ready
     */
    [[nodiscard]] bool isModelLoaded() const;

    /**
     * @brief We get the name/path of the currently loaded model
     */
    [[nodiscard]] QString modelPath() const;

    /**
     * @brief We set the language for transcription (default "en", use "auto" for detection)
     */
    void setLanguage(const QString& lang);

    /**
     * @brief We enable or disable translation to English
     */
    void setTranslateToEnglish(bool enabled);

    /**
     * @brief We set the maximum number of concurrent transcription tasks
     */
    void setMaxConcurrentTasks(int max);

    /**
     * @brief We check how many transcription tasks are currently running
     */
    [[nodiscard]] int activeTaskCount() const;

    /**
     * @brief We enable or disable the transcriber
     */
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

public slots:
    /**
     * @brief We receive a voice detection event and queue it for transcription
     */
    void onVoiceDetected(const VoiceDetectionEvent& event);

signals:
    /**
     * @brief We emit when a transcription completes successfully
     */
    void transcriptionReady(const TranscriptionResult& result);

    /**
     * @brief We emit when a transcription task fails
     */
    void transcriptionFailed(const QString& reason, double timestamp);

    /**
     * @brief We emit status updates for the UI
     */
    void statusUpdated(const QString& status);

private:
    /**
     * @brief We perform the actual Whisper transcription (runs on worker thread)
     * @param audioData Audio samples (float32, 16kHz mono)
     * @param event Source detection event for metadata
     */
    void performTranscription(const std::vector<float>& audioData,
                              const VoiceDetectionEvent& event);

#if WHISPER_AVAILABLE
    struct whisper_context* m_ctx{nullptr};
#else
    void* m_ctx{nullptr};
#endif

    QString m_modelPath;
    QString m_language{"en"};
    bool m_translateToEnglish{false};
    bool m_enabled{true};
    std::atomic<int> m_activeTasks{0};
    int m_maxConcurrentTasks{2};
    QMutex m_whisperMutex;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_WHISPER_TRANSCRIBER_H
