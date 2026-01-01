/**
 * @file src/core/AudioDemodulator.h
 * @title AudioDemodulator - IQ to Audio Demodulation Pipeline
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We demodulate raw IQ samples from the SDR into audible PCM audio using
 *          AM envelope detection, FM discriminator, and SSB methods.
 * @reason Each frequency band may use a different modulation scheme. We need a clean
 *         demodulation pipeline that produces consistent audio quality across modes.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with AM, NFM, WFM, USB, LSB demodulation
 */

#ifndef BTHL_SPIRITBOX_AUDIO_DEMODULATOR_H
#define BTHL_SPIRITBOX_AUDIO_DEMODULATOR_H

#include "core/SweepProfile.h"
#include <QObject>
#include <complex>
#include <vector>
#include <cstdint>

namespace bthl::spiritbox {

/**
 * @class AudioDemodulator
 * @purpose We convert complex IQ samples into real-valued PCM audio samples
 *          using the appropriate demodulation algorithm for each mode.
 */
class AudioDemodulator : public QObject {
    Q_OBJECT

public:
    explicit AudioDemodulator(QObject* parent = nullptr);
    ~AudioDemodulator() override = default;

    /**
     * @brief We set the SDR sample rate for accurate demodulation math
     * @param rate SDR sample rate in Hz
     */
    void setSdrSampleRate(double rate);

    /**
     * @brief We set the target audio output sample rate
     * @param rate Audio output rate in Hz (e.g., 16000 for Whisper, 48000 for playback)
     */
    void setAudioSampleRate(uint32_t rate);

    /**
     * @brief We set the demodulation filter bandwidth
     * @param bw Bandwidth in Hz
     */
    void setFilterBandwidth(double bw);

public slots:
    /**
     * @brief We receive IQ samples from the SweepEngine and demodulate them
     * @param samples Complex float IQ samples
     * @param freqHz Center frequency these samples were captured at
     * @param mode Demodulation mode to use
     */
    void processIQSamples(const std::vector<std::complex<float>>& samples,
                          double freqHz, DemodulationMode mode);

signals:
    /**
     * @brief We emit demodulated PCM audio samples (float, mono, normalized -1.0 to 1.0)
     * @param audioSamples Demodulated audio
     * @param freqHz The frequency these audio samples originated from
     * @param sampleRate The audio sample rate
     */
    void audioReady(const std::vector<float>& audioSamples, double freqHz, uint32_t sampleRate);

private:
    /**
     * @brief We demodulate AM using envelope detection (magnitude of complex signal)
     */
    std::vector<float> demodulateAM(const std::vector<std::complex<float>>& samples);

    /**
     * @brief We demodulate narrowband FM using quadrature discriminator
     */
    std::vector<float> demodulateNFM(const std::vector<std::complex<float>>& samples);

    /**
     * @brief We demodulate wideband FM using the same discriminator with wider bandwidth
     */
    std::vector<float> demodulateWFM(const std::vector<std::complex<float>>& samples);

    /**
     * @brief We demodulate USB (Upper Sideband) using Hilbert transform
     */
    std::vector<float> demodulateUSB(const std::vector<std::complex<float>>& samples);

    /**
     * @brief We demodulate LSB (Lower Sideband) using Hilbert transform
     */
    std::vector<float> demodulateLSB(const std::vector<std::complex<float>>& samples);

    /**
     * @brief We apply a simple low-pass filter to the demodulated audio
     * @param samples Audio samples to filter
     * @param cutoffHz Cutoff frequency in Hz
     * @param sampleRate Sample rate of the audio
     */
    void applyLowPassFilter(std::vector<float>& samples, double cutoffHz, double sampleRate);

    /**
     * @brief We decimate the audio from SDR sample rate to target audio rate
     * @param samples Input samples at SDR rate
     * @return Decimated samples at audio output rate
     */
    std::vector<float> decimateToAudioRate(const std::vector<float>& samples);

    /**
     * @brief We normalize audio samples to prevent clipping
     */
    void normalizeAudio(std::vector<float>& samples);

    /**
     * @brief We apply DC removal to eliminate DC offset from demodulation
     */
    void removeDCOffset(std::vector<float>& samples);

    double m_sdrSampleRate{2.4e6};
    uint32_t m_audioSampleRate{16000};
    double m_filterBandwidth{10000.0};

    // We maintain state for FM discriminator (previous sample)
    std::complex<float> m_prevFmSample{0.0f, 0.0f};

    // We maintain state for DC removal IIR filter
    float m_dcAlpha{0.995f};
    float m_dcPrevIn{0.0f};
    float m_dcPrevOut{0.0f};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_AUDIO_DEMODULATOR_H
