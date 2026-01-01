/**
 * @file src/emf/EMFCorrelator.cpp
 * @title EMFCorrelator Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement temporal correlation between EMF spikes, voice detections,
 *          and transcription results to identify multi-sensor coincident events.
 * @reason Multi-sensor correlation is the scientific backbone of our investigation platform.
 *         Single-sensor events are ambiguous; correlated multi-sensor events carry real weight.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with sliding window correlation
 */

#include "emf/EMFCorrelator.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace bthl::spiritbox {

EMFCorrelator::EMFCorrelator(QObject* parent)
    : QObject(parent) {
    qInfo() << "EMFCorrelator: We initialized the multi-sensor correlation engine";
}

void EMFCorrelator::setCorrelationWindowMs(uint32_t windowMs) {
    m_correlationWindowMs = windowMs;
}

uint32_t EMFCorrelator::correlationWindowMs() const {
    return m_correlationWindowMs;
}

void EMFCorrelator::setMinEMFDeviation(double deviationMg) {
    m_minEMFDeviation = deviationMg;
}

uint32_t EMFCorrelator::correlatedEventCount() const {
    return static_cast<uint32_t>(m_eventHistory.size());
}

const std::vector<CorrelatedEvent>& EMFCorrelator::eventHistory() const {
    return m_eventHistory;
}

void EMFCorrelator::reset() {
    m_pendingEMFSpikes.clear();
    m_pendingVoiceEvents.clear();
    m_pendingTranscriptions.clear();
    m_eventHistory.clear();
    qInfo() << "EMFCorrelator: We reset the correlator state for a new session";
}

void EMFCorrelator::onEMFSpike(const EMFReading& reading) {
    m_pendingEMFSpikes.push_back(reading);

    while (m_pendingEMFSpikes.size() > MAX_PENDING_EVENTS) {
        m_pendingEMFSpikes.pop_front();
    }

    checkCorrelation();
}

void EMFCorrelator::onEMFReading(const EMFReading& /*reading*/) {
    // We use continuous readings for baseline tracking but do not correlate them
    purgeExpiredEvents();
}

void EMFCorrelator::onVoiceDetected(const VoiceDetectionEvent& event) {
    m_pendingVoiceEvents.push_back(event);

    while (m_pendingVoiceEvents.size() > MAX_PENDING_EVENTS) {
        m_pendingVoiceEvents.pop_front();
    }

    checkCorrelation();
}

void EMFCorrelator::onTranscriptionReady(const TranscriptionResult& result) {
    m_pendingTranscriptions.push_back(result);

    while (m_pendingTranscriptions.size() > MAX_PENDING_EVENTS) {
        m_pendingTranscriptions.pop_front();
    }

    // We try to attach the transcription to a recent correlated event
    double windowSec = static_cast<double>(m_correlationWindowMs) / 1000.0 * 2.0;

    for (auto it = m_eventHistory.rbegin(); it != m_eventHistory.rend(); ++it) {
        if (!it->hasTranscription) {
            double delta = std::fabs(it->timestamp - result.timestamp);
            if (delta <= windowSec) {
                it->transcription = result;
                it->hasTranscription = true;
                qInfo() << "EMFCorrelator: We attached transcription to correlated event:"
                        << result.text;
                emit correlatedEventDetected(*it);
                break;
            }
        }
    }
}

void EMFCorrelator::checkCorrelation() {
    double windowSec = static_cast<double>(m_correlationWindowMs) / 1000.0;

    for (auto emfIt = m_pendingEMFSpikes.begin(); emfIt != m_pendingEMFSpikes.end();) {
        bool matched = false;

        for (auto voiceIt = m_pendingVoiceEvents.begin();
             voiceIt != m_pendingVoiceEvents.end();) {

            double timeDelta = std::fabs(emfIt->timestamp - voiceIt->timestamp);

            if (timeDelta <= windowSec) {
                CorrelatedEvent correlated;
                correlated.timestamp = (emfIt->timestamp + voiceIt->timestamp) / 2.0;
                correlated.emfReading = *emfIt;
                correlated.voiceEvent = *voiceIt;
                correlated.timeDeltaMs = timeDelta * 1000.0;
                correlated.correlationScore = calculateCorrelationScore(*emfIt, *voiceIt);

                // We check for a pending transcription within the window
                for (auto transIt = m_pendingTranscriptions.begin();
                     transIt != m_pendingTranscriptions.end(); ++transIt) {
                    double transDelta = std::fabs(correlated.timestamp - transIt->timestamp);
                    if (transDelta <= windowSec * 2.0) {
                        correlated.transcription = *transIt;
                        correlated.hasTranscription = true;
                        m_pendingTranscriptions.erase(transIt);
                        break;
                    }
                }

                m_eventHistory.push_back(correlated);

                QString logMsg = QString("CORRELATED EVENT #%1: EMF=%2mG + Voice@%3MHz "
                                         "(delta=%4ms, score=%5)")
                    .arg(m_eventHistory.size())
                    .arg(correlated.emfReading.emfMilligauss, 0, 'f', 2)
                    .arg(correlated.voiceEvent.frequencyHz / 1e6, 0, 'f', 3)
                    .arg(correlated.timeDeltaMs, 0, 'f', 1)
                    .arg(correlated.correlationScore, 0, 'f', 3);

                if (correlated.hasTranscription) {
                    logMsg += QString(" TEXT: \"%1\"").arg(correlated.transcription.text);
                }

                qInfo() << "EMFCorrelator:" << logMsg;
                emit correlatedEventDetected(correlated);
                emit statusUpdated(logMsg);

                voiceIt = m_pendingVoiceEvents.erase(voiceIt);
                matched = true;
                break;
            } else {
                ++voiceIt;
            }
        }

        if (matched) {
            emfIt = m_pendingEMFSpikes.erase(emfIt);
        } else {
            ++emfIt;
        }
    }

    purgeExpiredEvents();
}

void EMFCorrelator::purgeExpiredEvents() {
    double maxAgeSec = static_cast<double>(m_correlationWindowMs) / 1000.0 * 2.0;

    auto purgeDeque = [maxAgeSec](auto& deque, auto timestampGetter) {
        if (deque.size() < 2) return;
        double newest = timestampGetter(deque.back());
        while (!deque.empty()) {
            double age = newest - timestampGetter(deque.front());
            if (age > maxAgeSec) {
                deque.pop_front();
            } else {
                break;
            }
        }
    };

    purgeDeque(m_pendingEMFSpikes, [](const EMFReading& r) { return r.timestamp; });
    purgeDeque(m_pendingVoiceEvents, [](const VoiceDetectionEvent& e) { return e.timestamp; });
    purgeDeque(m_pendingTranscriptions, [](const TranscriptionResult& t) { return t.timestamp; });
}

float EMFCorrelator::calculateCorrelationScore(const EMFReading& emf,
                                                const VoiceDetectionEvent& voice) const {
    double windowSec = static_cast<double>(m_correlationWindowMs) / 1000.0;
    double timeDelta = std::fabs(emf.timestamp - voice.timestamp);

    // We score temporal proximity: closer in time scores higher
    float temporalScore = 1.0f - static_cast<float>(timeDelta / windowSec);
    temporalScore = std::clamp(temporalScore, 0.0f, 1.0f);

    // We score EMF strength relative to our deviation threshold
    float emfScore = std::clamp(
        static_cast<float>(emf.emfMilligauss / (m_minEMFDeviation * 5.0)),
        0.0f, 1.0f);

    // We use the VAD confidence directly
    float voiceScore = voice.confidence;

    // We weight: temporal 40%, EMF 30%, voice 30%
    float score = 0.40f * temporalScore + 0.30f * emfScore + 0.30f * voiceScore;

    return std::clamp(score, 0.0f, 1.0f);
}

} // namespace bthl::spiritbox
