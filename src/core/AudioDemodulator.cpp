/**
 * @file src/core/AudioDemodulator.cpp
 * @title AudioDemodulator Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement real-time demodulation of IQ samples into audible PCM audio.
 * @reason Clean audio output is critical for both human monitoring and Whisper transcription accuracy.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with AM envelope, FM discriminator, SSB demod
 */

#include "core/AudioDemodulator.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bthl::spiritbox {

AudioDemodulator::AudioDemodulator(QObject* parent)
    : QObject(parent) {
    qInfo() << "AudioDemodulator: We initialized the demodulation pipeline";
}

void AudioDemodulator::setSdrSampleRate(double rate) {
    m_sdrSampleRate = rate;
}

void AudioDemodulator::setAudioSampleRate(uint32_t rate) {
    m_audioSampleRate = rate;
}

void AudioDemodulator::setFilterBandwidth(double bw) {
    m_filterBandwidth = bw;
}

void AudioDemodulator::processIQSamples(const std::vector<std::complex<float>>& samples,
                                         double freqHz, DemodulationMode mode) {
    if (samples.empty()) return;

    std::vector<float> audio;

    // We select the appropriate demodulation algorithm based on mode
    switch (mode) {
        case DemodulationMode::AM:
            audio = demodulateAM(samples);
            break;
        case DemodulationMode::NFM:
            audio = demodulateNFM(samples);
            break;
        case DemodulationMode::WFM:
            audio = demodulateWFM(samples);
            break;
        case DemodulationMode::USB:
            audio = demodulateUSB(samples);
            break;
        case DemodulationMode::LSB:
            audio = demodulateLSB(samples);
            break;
        case DemodulationMode::RAW:
            // We pass through the real component of IQ samples
            audio.resize(samples.size());
            for (size_t i = 0; i < samples.size(); ++i) {
                audio[i] = samples[i].real();
            }
            break;
    }

    if (audio.empty()) return;

    // We apply DC removal to clean up the demodulated signal
    removeDCOffset(audio);

    // We decimate from SDR sample rate down to audio output rate
    audio = decimateToAudioRate(audio);

    // We normalize to prevent clipping
    normalizeAudio(audio);

    emit audioReady(audio, freqHz, m_audioSampleRate);
}

// ─── Demodulation Algorithms ───────────────────────────────────────────────────

std::vector<float> AudioDemodulator::demodulateAM(
    const std::vector<std::complex<float>>& samples) {
    // We use envelope detection: audio = |IQ| = sqrt(I^2 + Q^2)
    std::vector<float> audio(samples.size());

    for (size_t i = 0; i < samples.size(); ++i) {
        audio[i] = std::abs(samples[i]);
    }

    // We remove the DC component (carrier level) from the envelope
    float mean = std::accumulate(audio.begin(), audio.end(), 0.0f)
                 / static_cast<float>(audio.size());
    for (auto& s : audio) {
        s -= mean;
    }

    // We apply a low-pass filter at the AM audio bandwidth
    applyLowPassFilter(audio, 5000.0, m_sdrSampleRate);

    return audio;
}

std::vector<float> AudioDemodulator::demodulateNFM(
    const std::vector<std::complex<float>>& samples) {
    // We use a quadrature FM discriminator: audio = arg(sample[n] * conj(sample[n-1]))
    std::vector<float> audio(samples.size());

    for (size_t i = 0; i < samples.size(); ++i) {
        std::complex<float> product = samples[i] * std::conj(m_prevFmSample);
        audio[i] = std::atan2(product.imag(), product.real());
        m_prevFmSample = samples[i];
    }

    // We apply a low-pass filter for narrowband FM audio (typically 3 kHz)
    applyLowPassFilter(audio, 3000.0, m_sdrSampleRate);

    return audio;
}

std::vector<float> AudioDemodulator::demodulateWFM(
    const std::vector<std::complex<float>>& samples) {
    // We use the same discriminator but with wider audio bandwidth for broadcast FM
    std::vector<float> audio(samples.size());

    for (size_t i = 0; i < samples.size(); ++i) {
        std::complex<float> product = samples[i] * std::conj(m_prevFmSample);
        audio[i] = std::atan2(product.imag(), product.real());
        m_prevFmSample = samples[i];
    }

    // We filter at 15 kHz for mono broadcast FM audio
    applyLowPassFilter(audio, 15000.0, m_sdrSampleRate);

    return audio;
}

std::vector<float> AudioDemodulator::demodulateUSB(
    const std::vector<std::complex<float>>& samples) {
    // We extract the upper sideband: audio = I*cos - Q*sin (simplified to I + Q rotation)
    // For USB we take the real part after mixing to baseband
    std::vector<float> audio(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        audio[i] = samples[i].real() - samples[i].imag();
    }
    applyLowPassFilter(audio, 3000.0, m_sdrSampleRate);
    return audio;
}

std::vector<float> AudioDemodulator::demodulateLSB(
    const std::vector<std::complex<float>>& samples) {
    // We extract the lower sideband: conjugate of USB
    std::vector<float> audio(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        audio[i] = samples[i].real() + samples[i].imag();
    }
    applyLowPassFilter(audio, 3000.0, m_sdrSampleRate);
    return audio;
}

// ─── Audio Processing Utilities ────────────────────────────────────────────────

void AudioDemodulator::applyLowPassFilter(std::vector<float>& samples,
                                           double cutoffHz, double sampleRate) {
    // We use a simple single-pole IIR low-pass filter for real-time efficiency
    // H(z) = alpha / (1 - (1-alpha) * z^-1)
    double rc = 1.0 / (2.0 * M_PI * cutoffHz);
    double dt = 1.0 / sampleRate;
    float alpha = static_cast<float>(dt / (rc + dt));

    float prev = samples[0];
    for (size_t i = 1; i < samples.size(); ++i) {
        samples[i] = prev + alpha * (samples[i] - prev);
        prev = samples[i];
    }
}

std::vector<float> AudioDemodulator::decimateToAudioRate(const std::vector<float>& samples) {
    // We calculate the decimation factor from SDR rate to audio rate
    int decimationFactor = static_cast<int>(m_sdrSampleRate / m_audioSampleRate);
    if (decimationFactor <= 1) {
        return samples; // No decimation needed
    }

    // We decimate by averaging groups of samples (simple box filter + downsample)
    size_t outputSize = samples.size() / static_cast<size_t>(decimationFactor);
    std::vector<float> decimated(outputSize);

    for (size_t i = 0; i < outputSize; ++i) {
        float sum = 0.0f;
        size_t start = i * static_cast<size_t>(decimationFactor);
        size_t end = std::min(start + static_cast<size_t>(decimationFactor), samples.size());
        for (size_t j = start; j < end; ++j) {
            sum += samples[j];
        }
        decimated[i] = sum / static_cast<float>(end - start);
    }

    return decimated;
}

void AudioDemodulator::normalizeAudio(std::vector<float>& samples) {
    if (samples.empty()) return;

    // We find the peak amplitude and scale to -1.0 to 1.0 range
    float peak = 0.0f;
    for (const auto& s : samples) {
        float absVal = std::fabs(s);
        if (absVal > peak) peak = absVal;
    }

    if (peak > 1e-6f) {
        float scale = 0.9f / peak; // We leave a little headroom
        for (auto& s : samples) {
            s *= scale;
        }
    }
}

void AudioDemodulator::removeDCOffset(std::vector<float>& samples) {
    // We use a DC blocking IIR filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
    for (auto& sample : samples) {
        float filtered = sample - m_dcPrevIn + m_dcAlpha * m_dcPrevOut;
        m_dcPrevIn = sample;
        m_dcPrevOut = filtered;
        sample = filtered;
    }
}

} // namespace bthl::spiritbox
