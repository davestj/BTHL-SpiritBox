/**
 * @file src/emf/EMFCorrelator.h
 * @title EMFCorrelator - Multi-Sensor Event Correlation Engine
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We correlate EMF readings with voice detection and transcription events
 *          to identify temporally coincident anomalies across multiple sensor streams.
 * @reason Temporal correlation between EMF spikes and audio anomalies provides stronger
 *         evidence of paranormal activity than either sensor alone.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with configurable correlation window
 */

#ifndef BTHL_SPIRITBOX_EMF_CORRELATOR_H
#define BTHL_SPIRITBOX_EMF_CORRELATOR_H

#include "emf/EMFSerialReader.h"
#include "detection/VoiceActivityDetector.h"
#include "detection/WhisperTranscriber.h"
#include <QObject>
#include <deque>
#include <vector>

namespace bthl::spiritbox {

/**
 * @struct CorrelatedEvent
 * @purpose We store events where multiple sensors triggered within the correlation window
 */
struct CorrelatedEvent {
    double timestamp;               ///< Average timestamp of correlated events
    EMFReading emfReading;          ///< The EMF spike data
    VoiceDetectionEvent voiceEvent; ///< The voice detection data
    TranscriptionResult transcription; ///< Whisper transcription (if available)
    bool hasTranscription{false};   ///< Whether we have a transcription for this event
    float correlationScore;         ///< Combined correlation strength (0.0 to 1.0)
    double timeDeltaMs;             ///< Time difference between EMF and audio event
};

/**
 * @class EMFCorrelator
 * @purpose We monitor both EMF and voice detection event streams and flag events
 *          that occur within a configurable time window of each other.
 */
class EMFCorrelator : public QObject {
    Q_OBJECT

public:
    explicit EMFCorrelator(QObject* parent = nullptr);
    ~EMFCorrelator() override = default;

    /**
     * @brief We set the correlation time window in milliseconds.
     *        EMF and voice events within this window are considered correlated.
     * @param windowMs Correlation window (default 500ms)
     */
    void setCorrelationWindowMs(uint32_t windowMs);
    [[nodiscard]] uint32_t correlationWindowMs() const;

    /**
     * @brief We set the minimum EMF deviation from baseline to qualify for correlation
     * @param deviationMg Minimum deviation in milligauss
     */
    void setMinEMFDeviation(double deviationMg);

    /**
     * @brief We get the total number of correlated events in this session
     */
    [[nodiscard]] uint32_t correlatedEventCount() const;

    /**
     * @brief We get the history of all correlated events
     */
    [[nodiscard]] const std::vector<CorrelatedEvent>& eventHistory() const;

    /**
     * @brief We reset the correlator state for a new session
     */
    void reset();

public slots:
    /**
     * @brief We receive EMF spike events from the serial reader
     */
    void onEMFSpike(const EMFReading& reading);

    /**
     * @brief We receive all EMF readings (not just spikes) for continuous monitoring
     */
    void onEMFReading(const EMFReading& reading);

    /**
     * @brief We receive voice detection events from the VAD
     */
    void onVoiceDetected(const VoiceDetectionEvent& event);

    /**
     * @brief We receive transcription results to attach to correlated events
     */
    void onTranscriptionReady(const TranscriptionResult& result);

signals:
    /**
     * @brief We emit when an EMF spike and voice detection correlate within the window
     */
    void correlatedEventDetected(const CorrelatedEvent& event);

    /**
     * @brief We emit status updates for the UI
     */
    void statusUpdated(const QString& status);

private:
    /**
     * @brief We check for correlation between the latest events in both queues
     */
    void checkCorrelation();

    /**
     * @brief We clean up expired events from the pending queues
     */
    void purgeExpiredEvents();

    /**
     * @brief We calculate a correlation score based on temporal proximity and signal strength
     */
    float calculateCorrelationScore(const EMFReading& emf,
                                     const VoiceDetectionEvent& voice) const;

    uint32_t m_correlationWindowMs{500};
    double m_minEMFDeviation{2.0};

    // We maintain pending event queues for temporal matching
    std::deque<EMFReading> m_pendingEMFSpikes;
    std::deque<VoiceDetectionEvent> m_pendingVoiceEvents;
    std::deque<TranscriptionResult> m_pendingTranscriptions;

    // We store the complete history of correlated events
    std::vector<CorrelatedEvent> m_eventHistory;

    static constexpr size_t MAX_PENDING_EVENTS = 100;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_EMF_CORRELATOR_H
