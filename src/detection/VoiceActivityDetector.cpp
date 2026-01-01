/**
 * @file src/detection/VoiceActivityDetector.cpp
 * @title VoiceActivityDetector Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement real-time voice activity detection using multi-feature analysis.
 * @reason Accurate VAD is critical for filtering noise from potential voice detections
 *         and for feeding clean audio segments to Whisper for transcription.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with three-feature confidence scoring
 */

#include "detection/VoiceActivityDetector.h"
#include <QDebug>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace bthl::spiritbox {

VoiceActivityDetector::VoiceActivityDetector(QObject* parent)
    : QObject(parent) {
    qInfo() << "VoiceActivityDetector: We initialized the voice activity detector";
}

void VoiceActivityDetector::setSampleRate(uint32_t rate) {
    m_sampleRate = rate;
    m_maxSnippetSamples = static_cast<size_t>(m_snippetDurationMs) * rate / 1000;
}

void VoiceActivityDetector::setEnergyThreshold(float thresholdDb) {
    m_energyThreshold = thresholdDb;
}

void VoiceActivityDetector::setConfidenceThreshold(float threshold) {
    m_confidenceThreshold = std::clamp(threshold, 0.0f, 1.0f);
}

void VoiceActivityDetector::setMinDurationMs(uint32_t ms) {
    m_minDurationMs = ms;
    // We recalculate the minimum consecutive frames needed
    // Assuming ~20ms frame size at our sample rate
    m_minConsecutiveFrames = std::max(1u, ms / 20);
}

void VoiceActivityDetector::setSnippetDurationMs(uint32_t ms) {
    m_snippetDurationMs = ms;
    m_maxSnippetSamples = static_cast<size_t>(ms) * m_sampleRate / 1000;
}

void VoiceActivityDetector::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool VoiceActivityDetector::isEnabled() const {
    return m_enabled;
}

void VoiceActivityDetector::processAudio(const std::vector<float>& samples,
                                          double freqHz, uint32_t sampleRate) {
    if (!m_enabled || samples.empty()) return;

    m_sampleRate = sampleRate;

    // We accumulate samples in our snippet buffer for capture
    for (float s : samples) {
        m_snippetBuffer.push_back(s);
    }
    while (m_snippetBuffer.size() > m_maxSnippetSamples) {
        m_snippetBuffer.pop_front();
    }

    // We update session time
    m_sessionTime += static_cast<double>(samples.size()) / static_cast<double>(sampleRate);

    // We calculate our three detection features
    float energyDb = calculateEnergyDb(samples);
    float zcr = calculateZeroCrossingRate(samples);
    float flatness = calculateSpectralFlatness(samples);
    float confidence = calculateConfidence(energyDb, zcr, flatness);

    // We emit metrics for the UI regardless of detection
    emit metricsUpdated(energyDb, zcr, flatness, confidence);

    // We check if this frame passes our energy gate
    if (energyDb < m_energyThreshold) {
        m_consecutiveVoiceFrames = 0;
        return;
    }

    // We check if confidence exceeds our threshold
    if (confidence >= m_confidenceThreshold) {
        m_consecutiveVoiceFrames++;

        // We only emit a detection event after sustained voice activity
        if (m_consecutiveVoiceFrames >= m_minConsecutiveFrames) {
            VoiceDetectionEvent event;
            event.timestamp = m_sessionTime;
            event.frequencyHz = freqHz;
            event.confidence = confidence;
            event.energyDb = energyDb;
            event.zeroCrossingRate = zcr;
            event.spectralFlatness = flatness;

            // We capture the snippet buffer for Whisper analysis
            event.audioSnippet.assign(m_snippetBuffer.begin(), m_snippetBuffer.end());

            emit voiceDetected(event);

            // We reset consecutive counter to avoid rapid-fire events
            m_consecutiveVoiceFrames = 0;
        }
    } else {
        m_consecutiveVoiceFrames = 0;
    }
}

// ─── Feature Extraction ────────────────────────────────────────────────────────

float VoiceActivityDetector::calculateEnergyDb(const std::vector<float>& frame) const {
    if (frame.empty()) return -120.0f;

    double sumSquared = 0.0;
    for (float s : frame) {
        sumSquared += static_cast<double>(s) * static_cast<double>(s);
    }
    double rms = std::sqrt(sumSquared / static_cast<double>(frame.size()));

    if (rms < 1e-12) return -120.0f;

    return static_cast<float>(20.0 * std::log10(rms));
}

float VoiceActivityDetector::calculateZeroCrossingRate(const std::vector<float>& frame) const {
    if (frame.size() < 2) return 0.0f;

    uint32_t crossings = 0;
    for (size_t i = 1; i < frame.size(); ++i) {
        if ((frame[i] >= 0.0f && frame[i - 1] < 0.0f) ||
            (frame[i] < 0.0f && frame[i - 1] >= 0.0f)) {
            crossings++;
        }
    }

    return static_cast<float>(crossings) / static_cast<float>(frame.size() - 1);
}

float VoiceActivityDetector::calculateSpectralFlatness(const std::vector<float>& frame) const {
    if (frame.size() < 2) return 1.0f;

    // We compute a simple approximation of spectral flatness
    // using the ratio of geometric mean to arithmetic mean of |sample| values
    // This avoids a full FFT for real-time performance

    double logSum = 0.0;
    double arithmeticSum = 0.0;
    size_t validCount = 0;

    for (float s : frame) {
        float absVal = std::fabs(s);
        if (absVal > 1e-10f) {
            logSum += std::log(static_cast<double>(absVal));
            arithmeticSum += static_cast<double>(absVal);
            validCount++;
        }
    }

    if (validCount < 2) return 1.0f;

    double geometricMean = std::exp(logSum / static_cast<double>(validCount));
    double arithmeticMean = arithmeticSum / static_cast<double>(validCount);

    if (arithmeticMean < 1e-12) return 1.0f;

    return static_cast<float>(std::clamp(geometricMean / arithmeticMean, 0.0, 1.0));
}

float VoiceActivityDetector::calculateConfidence(float energyDb, float zcr,
                                                  float flatness) const {
    // We combine features with weighted scoring
    // Voice characteristics:
    //   - Energy: above threshold (already gated)
    //   - ZCR: typically 0.02 to 0.15 for speech
    //   - Spectral flatness: lower for voice (tonal) vs noise (flat)

    float energyScore = 0.0f;
    // We scale energy from threshold to threshold+30dB into 0.0-1.0
    float energyRange = energyDb - m_energyThreshold;
    if (energyRange > 0.0f) {
        energyScore = std::clamp(energyRange / 30.0f, 0.0f, 1.0f);
    }

    // We score ZCR: peak confidence at 0.05-0.10 (typical speech)
    float zcrScore = 0.0f;
    if (zcr >= 0.02f && zcr <= 0.20f) {
        if (zcr <= 0.10f) {
            zcrScore = (zcr - 0.02f) / 0.08f; // Ramp up from 0.02 to 0.10
        } else {
            zcrScore = 1.0f - (zcr - 0.10f) / 0.10f; // Ramp down from 0.10 to 0.20
        }
        zcrScore = std::clamp(zcrScore, 0.0f, 1.0f);
    }

    // We score flatness: lower is more voice-like
    float flatnessScore = 1.0f - flatness; // Invert so voice scores higher

    // We combine with weights: energy 30%, ZCR 30%, flatness 40%
    float confidence = 0.30f * energyScore + 0.30f * zcrScore + 0.40f * flatnessScore;

    return std::clamp(confidence, 0.0f, 1.0f);
}

} // namespace bthl::spiritbox
