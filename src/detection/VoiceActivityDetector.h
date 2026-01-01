/**
 * @file src/detection/VoiceActivityDetector.h
 * @title VoiceActivityDetector - Real-time Voice Activity Detection
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We analyze demodulated audio in real-time to detect the presence of voice-like
 *          signals using energy thresholding, zero-crossing rate, and spectral flatness.
 * @reason We need to flag audio segments that contain potential voice content for further
 *         analysis by Whisper and for correlation with EMF events.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with multi-feature VAD
 */

#ifndef BTHL_SPIRITBOX_VOICE_ACTIVITY_DETECTOR_H
#define BTHL_SPIRITBOX_VOICE_ACTIVITY_DETECTOR_H

#include <QObject>
#include <vector>
#include <deque>
#include <cstdint>

namespace bthl::spiritbox {

/**
 * @struct VoiceDetectionEvent
 * @purpose We emit this when voice activity is detected in the audio stream
 */
struct VoiceDetectionEvent {
    double timestamp;           ///< When we detected the voice (seconds since session start)
    double frequencyHz;         ///< The RF frequency where we detected voice
    float confidence;           ///< Our confidence level (0.0 to 1.0)
    float energyDb;             ///< Signal energy in dB
    float zeroCrossingRate;     ///< Zero crossing rate (voice typically 0.02-0.15)
    float spectralFlatness;     ///< Spectral flatness (voice < noise)
    std::vector<float> audioSnippet; ///< We capture the audio snippet for Whisper
};

/**
 * @class VoiceActivityDetector
 * @purpose We implement a multi-feature voice activity detector that analyzes
 *          incoming audio frames and flags segments likely containing speech.
 */
class VoiceActivityDetector : public QObject {
    Q_OBJECT

public:
    explicit VoiceActivityDetector(QObject* parent = nullptr);
    ~VoiceActivityDetector() override = default;

    /**
     * @brief We set the audio sample rate for accurate feature calculation
     */
    void setSampleRate(uint32_t rate);

    /**
     * @brief We set the minimum energy threshold for voice detection (in dB)
     * @param thresholdDb Energy threshold; signals below this are ignored
     */
    void setEnergyThreshold(float thresholdDb);

    /**
     * @brief We set the minimum confidence threshold for emitting detection events
     * @param threshold Confidence threshold (0.0 to 1.0)
     */
    void setConfidenceThreshold(float threshold);

    /**
     * @brief We set the minimum duration of voice activity to trigger detection (ms)
     */
    void setMinDurationMs(uint32_t ms);

    /**
     * @brief We set the audio snippet capture duration for Whisper analysis (ms)
     */
    void setSnippetDurationMs(uint32_t ms);

    /**
     * @brief We enable or disable the VAD
     */
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

public slots:
    /**
     * @brief We process incoming audio samples for voice detection
     * @param samples Demodulated audio samples
     * @param freqHz RF frequency source
     * @param sampleRate Audio sample rate
     */
    void processAudio(const std::vector<float>& samples, double freqHz, uint32_t sampleRate);

signals:
    /**
     * @brief We emit when voice activity is detected above our confidence threshold
     */
    void voiceDetected(const VoiceDetectionEvent& event);

    /**
     * @brief We emit the current VAD metrics for the UI display
     */
    void metricsUpdated(float energy, float zcr, float flatness, float confidence);

private:
    /**
     * @brief We calculate the RMS energy of an audio frame in dB
     */
    float calculateEnergyDb(const std::vector<float>& frame) const;

    /**
     * @brief We calculate the zero-crossing rate of an audio frame
     *        Voice typically has ZCR between 0.02 and 0.15
     */
    float calculateZeroCrossingRate(const std::vector<float>& frame) const;

    /**
     * @brief We calculate spectral flatness using the geometric/arithmetic mean ratio
     *        of the power spectrum. Voice has lower flatness than noise.
     */
    float calculateSpectralFlatness(const std::vector<float>& frame) const;

    /**
     * @brief We combine all features into a single confidence score
     */
    float calculateConfidence(float energyDb, float zcr, float flatness) const;

    uint32_t m_sampleRate{16000};
    float m_energyThreshold{-40.0f};
    float m_confidenceThreshold{0.5f};
    uint32_t m_minDurationMs{100};
    uint32_t m_snippetDurationMs{2000};
    bool m_enabled{true};

    // We accumulate audio samples for snippet capture
    std::deque<float> m_snippetBuffer;
    size_t m_maxSnippetSamples{32000}; // 2 seconds at 16kHz

    // We track consecutive voice frames for duration gating
    uint32_t m_consecutiveVoiceFrames{0};
    uint32_t m_minConsecutiveFrames{3};

    // We track session time
    double m_sessionTime{0.0};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_VOICE_ACTIVITY_DETECTOR_H
