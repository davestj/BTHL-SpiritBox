/**
 * @file src/detection/WhisperTranscriber.cpp
 * @title WhisperTranscriber Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement local speech-to-text using whisper.cpp with async processing.
 * @reason Async transcription prevents blocking the audio pipeline while Whisper processes
 *         each audio snippet, keeping the sweep and detection running smoothly.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with QtConcurrent async execution
 */

#include "detection/WhisperTranscriber.h"
#include <QDebug>
#include <QtConcurrent>
#include <QElapsedTimer>

namespace bthl::spiritbox {

WhisperTranscriber::WhisperTranscriber(QObject* parent)
    : QObject(parent) {
#if WHISPER_AVAILABLE
    qInfo() << "WhisperTranscriber: We initialized with whisper.cpp support enabled";
#else
    qWarning() << "WhisperTranscriber: We initialized WITHOUT whisper.cpp (transcription disabled)";
#endif
}

WhisperTranscriber::~WhisperTranscriber() {
    unloadModel();
}

bool WhisperTranscriber::loadModel(const QString& modelPath) {
#if WHISPER_AVAILABLE
    QMutexLocker lock(&m_whisperMutex);

    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }

    emit statusUpdated(QString("We are loading Whisper model: %1").arg(modelPath));

    struct whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(modelPath.toStdString().c_str(), cparams);

    if (!m_ctx) {
        emit statusUpdated("We failed to load the Whisper model");
        return false;
    }

    m_modelPath = modelPath;
    emit statusUpdated(QString("We loaded Whisper model successfully"));
    qInfo() << "WhisperTranscriber: We loaded model from" << modelPath;
    return true;
#else
    Q_UNUSED(modelPath)
    qWarning() << "WhisperTranscriber: We cannot load model because whisper.cpp is not available";
    return false;
#endif
}

void WhisperTranscriber::unloadModel() {
#if WHISPER_AVAILABLE
    QMutexLocker lock(&m_whisperMutex);
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_modelPath.clear();
        qInfo() << "WhisperTranscriber: We unloaded the Whisper model";
    }
#endif
}

bool WhisperTranscriber::isModelLoaded() const {
    return m_ctx != nullptr;
}

QString WhisperTranscriber::modelPath() const {
    return m_modelPath;
}

void WhisperTranscriber::setLanguage(const QString& lang) {
    m_language = lang;
}

void WhisperTranscriber::setTranslateToEnglish(bool enabled) {
    m_translateToEnglish = enabled;
}

void WhisperTranscriber::setMaxConcurrentTasks(int max) {
    m_maxConcurrentTasks = max;
}

int WhisperTranscriber::activeTaskCount() const {
    return m_activeTasks.load();
}

void WhisperTranscriber::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool WhisperTranscriber::isEnabled() const {
    return m_enabled;
}

void WhisperTranscriber::onVoiceDetected(const VoiceDetectionEvent& event) {
    if (!m_enabled || !m_ctx) return;

    // We limit concurrent transcription tasks to prevent CPU overload
    if (m_activeTasks.load() >= m_maxConcurrentTasks) {
        qDebug() << "WhisperTranscriber: We are skipping transcription (max concurrent tasks reached)";
        return;
    }

    // We launch the transcription on a background thread
    m_activeTasks++;
    QtConcurrent::run([this, event]() {
        performTranscription(event.audioSnippet, event);
        m_activeTasks--;
    });
}

void WhisperTranscriber::performTranscription(const std::vector<float>& audioData,
                                               const VoiceDetectionEvent& event) {
#if WHISPER_AVAILABLE
    QMutexLocker lock(&m_whisperMutex);

    if (!m_ctx || audioData.empty()) {
        emit transcriptionFailed("We have no model loaded or empty audio data", event.timestamp);
        return;
    }

    QElapsedTimer timer;
    timer.start();

    // We configure Whisper inference parameters
    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = false;
    wparams.print_special = false;
    wparams.translate = m_translateToEnglish;
    wparams.no_context = true;
    wparams.single_segment = true;
    wparams.n_threads = 2; // We limit threads to avoid starving the audio pipeline

    std::string langStr = m_language.toStdString();
    if (m_language != "auto") {
        wparams.language = langStr.c_str();
    }

    // We run the Whisper inference
    int ret = whisper_full(m_ctx, wparams, audioData.data(),
                           static_cast<int>(audioData.size()));

    if (ret != 0) {
        emit transcriptionFailed(
            QString("We encountered a Whisper inference error (code %1)").arg(ret),
            event.timestamp);
        return;
    }

    // We extract the transcription result
    int numSegments = whisper_full_n_segments(m_ctx);
    QString transcribedText;
    float avgProbability = 0.0f;

    for (int i = 0; i < numSegments; ++i) {
        const char* text = whisper_full_get_segment_text(m_ctx, i);
        if (text) {
            transcribedText += QString::fromUtf8(text).trimmed();
        }

        // We accumulate token probabilities for an average confidence
        int numTokens = whisper_full_n_tokens(m_ctx, i);
        for (int t = 0; t < numTokens; ++t) {
            avgProbability += whisper_full_get_token_p(m_ctx, i, t);
        }
        if (numTokens > 0) {
            avgProbability /= static_cast<float>(numTokens);
        }
    }

    // We only emit results that contain actual text
    transcribedText = transcribedText.trimmed();
    if (!transcribedText.isEmpty() && transcribedText != "[BLANK_AUDIO]") {
        TranscriptionResult result;
        result.text = transcribedText;
        result.timestamp = event.timestamp;
        result.frequencyHz = event.frequencyHz;
        result.confidence = event.confidence;
        result.whisperProbability = avgProbability;
        result.language = m_language;
        result.processingTimeMs = timer.elapsed();
        result.source = event.source;

        qInfo() << "WhisperTranscriber: We transcribed at"
                << event.frequencyHz / 1e6 << "MHz:"
                << transcribedText
                << "(prob:" << avgProbability << ")";

        emit transcriptionReady(result);
    }

#else
    Q_UNUSED(audioData)
    emit transcriptionFailed("We cannot transcribe because whisper.cpp is not compiled in",
                             event.timestamp);
#endif
}

} // namespace bthl::spiritbox
