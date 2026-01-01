/**
 * @file src/core/SweepProfile.h
 * @title SweepProfile - Frequency Sweep Configuration
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose Defines configurable sweep profiles for frequency scanning operations.
 *          We use these profiles to control how the SDR hops across frequency bands,
 *          including start/stop frequencies, step size, dwell time, and demodulation mode.
 * @reason Parameterizing sweep behavior allows us to create reusable scan profiles
 *         for different use cases: AM broadcast, FM broadcast, VHF, and custom ranges.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with JSON serialization support
 */

#ifndef BTHL_SPIRITBOX_SWEEP_PROFILE_H
#define BTHL_SPIRITBOX_SWEEP_PROFILE_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <vector>
#include <cstdint>

namespace bthl::spiritbox {

/**
 * @enum DemodulationMode
 * @purpose We define the supported demodulation modes for extracting audio from RF signals
 */
enum class DemodulationMode : uint8_t {
    AM = 0,     ///< Amplitude Modulation (envelope detection)
    NFM,        ///< Narrowband FM (discriminator demod)
    WFM,        ///< Wideband FM (broadcast FM)
    USB,        ///< Upper Sideband
    LSB,        ///< Lower Sideband
    RAW         ///< Raw IQ passthrough (no demodulation)
};

/**
 * @struct SweepBand
 * @purpose We define a single frequency band within a sweep profile. A profile
 *          can contain multiple bands to scan non-contiguous ranges.
 */
struct SweepBand {
    double startFreqHz;         ///< We start scanning at this frequency (Hz)
    double stopFreqHz;          ///< We stop scanning at this frequency (Hz)
    double stepSizeHz;          ///< We advance by this amount each hop (Hz)
    DemodulationMode demodMode; ///< We use this demod mode for the band
    double bandwidthHz;         ///< We set the SDR filter bandwidth (Hz)

    /**
     * @brief We calculate the total number of frequency steps in this band
     * @return Number of discrete frequency hops
     */
    [[nodiscard]] uint32_t stepCount() const;

    /**
     * @brief We serialize this band to JSON for profile storage
     */
    [[nodiscard]] QJsonObject toJson() const;

    /**
     * @brief We deserialize a band from JSON
     */
    static SweepBand fromJson(const QJsonObject& json);
};

/**
 * @class SweepProfile
 * @purpose We use this class to define complete sweep configurations that can be
 *          saved, loaded, and applied to the SweepEngine. Each profile contains
 *          one or more frequency bands, timing parameters, and metadata.
 */
class SweepProfile {
public:
    SweepProfile() = default;
    explicit SweepProfile(const QString& name);

    // ─── Profile Metadata ──────────────────────────────────────────────────

    void setName(const QString& name);
    [[nodiscard]] QString name() const;

    void setDescription(const QString& desc);
    [[nodiscard]] QString description() const;

    // ─── Timing Configuration ──────────────────────────────────────────────

    /**
     * @brief We set how long the SDR dwells on each frequency step (milliseconds).
     *        Lower values produce faster sweeps. Typical spirit box range is 10-100ms.
     * @param ms Dwell time in milliseconds
     */
    void setDwellTimeMs(uint32_t ms);
    [[nodiscard]] uint32_t dwellTimeMs() const;

    /**
     * @brief We set the audio sample rate for demodulated output.
     *        Standard rates: 8000, 16000, 22050, 44100, 48000
     * @param rate Sample rate in Hz
     */
    void setAudioSampleRate(uint32_t rate);
    [[nodiscard]] uint32_t audioSampleRate() const;

    /**
     * @brief We set the SDR sample rate for IQ capture.
     *        Must be at least 2x the widest bandwidth in any band.
     * @param rate SDR sample rate in Hz
     */
    void setSdrSampleRate(double rate);
    [[nodiscard]] double sdrSampleRate() const;

    /**
     * @brief We set the SDR gain. Use 0 for automatic gain control.
     * @param gain Gain value in dB, or 0 for AGC
     */
    void setGain(double gain);
    [[nodiscard]] double gain() const;

    // ─── Band Management ───────────────────────────────────────────────────

    void addBand(const SweepBand& band);
    void removeBand(size_t index);
    void clearBands();
    [[nodiscard]] const std::vector<SweepBand>& bands() const;
    [[nodiscard]] size_t bandCount() const;

    /**
     * @brief We calculate the total number of frequency steps across all bands
     */
    [[nodiscard]] uint32_t totalSteps() const;

    /**
     * @brief We calculate the estimated time for one complete sweep cycle
     * @return Duration in milliseconds
     */
    [[nodiscard]] uint32_t estimatedCycleTimeMs() const;

    // ─── Sweep Behavior ────────────────────────────────────────────────────

    /**
     * @brief We enable reverse sweep direction on alternating passes (ping-pong mode)
     */
    void setBidirectional(bool enabled);
    [[nodiscard]] bool isBidirectional() const;

    /**
     * @brief We enable randomized step order within each band for non-sequential scanning
     */
    void setRandomized(bool enabled);
    [[nodiscard]] bool isRandomized() const;

    // ─── Serialization ─────────────────────────────────────────────────────

    [[nodiscard]] QJsonObject toJson() const;
    static SweepProfile fromJson(const QJsonObject& json);

    bool saveToFile(const QString& filePath) const;
    static SweepProfile loadFromFile(const QString& filePath);

    // ─── Factory Presets ───────────────────────────────────────────────────

    static SweepProfile createAMBroadcast();
    static SweepProfile createFMBroadcast();
    static SweepProfile createFullSpectrum();
    static SweepProfile createVHFLow();

private:
    QString m_name{"Untitled"};
    QString m_description;
    std::vector<SweepBand> m_bands;
    uint32_t m_dwellTimeMs{50};
    uint32_t m_audioSampleRate{16000};
    double m_sdrSampleRate{2.4e6};
    double m_gain{0.0};
    bool m_bidirectional{false};
    bool m_randomized{false};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SWEEP_PROFILE_H
