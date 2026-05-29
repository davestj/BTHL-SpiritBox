/**
 * @file src/core/SweepEngine.h
 * @title SweepEngine - SDR Frequency Sweep Controller
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We use this engine to control the SoapySDR device for rapid frequency sweeping.
 *          It manages device initialization, frequency hopping, IQ sample capture, and
 *          feeds raw samples to the AudioDemodulator for real-time audio extraction.
 * @reason The sweep engine is the heart of the spirit box. We need precise timing control
 *         over frequency hops and continuous IQ sample streaming to produce the characteristic
 *         rapid-scan audio output.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with SoapySDR multi-device support
 */

#ifndef BTHL_SPIRITBOX_SWEEP_ENGINE_H
#define BTHL_SPIRITBOX_SWEEP_ENGINE_H

#include "core/SweepProfile.h"
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <complex>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

namespace bthl::spiritbox {

/**
 * @struct SdrDeviceInfo
 * @purpose We store discovered SDR device metadata for user selection
 */
struct SdrDeviceInfo {
    QString label;              ///< Human readable device label
    QString driver;             ///< SoapySDR driver name
    QString serial;             ///< Device serial number
    SoapySDR::Kwargs kwargs;    ///< Full device arguments for SoapySDR::Device::make()
};

/**
 * @struct SdrCapabilities
 * @purpose We capture what the *actual* connected SDR can physically do, probed directly from
 *          the hardware via SoapySDR. We use this to skip unreachable frequencies and to expose
 *          the device's real limits (e.g. an E4000 cannot tune the AM band).
 */
struct SdrCapabilities {
    bool valid{false};          ///< True once we have probed a live device
    QString driver;             ///< SoapySDR driver (e.g. "rtlsdr")
    QString hardwareKey;        ///< Hardware key string
    QString tuner;              ///< Tuner chip (e.g. "Elonics E4000", "Rafael Micro R820T")
    double freqMinHz{0.0};      ///< Lowest tunable frequency (Hz)
    double freqMaxHz{0.0};      ///< Highest tunable frequency (Hz)
    double sampleRateMinHz{0.0};///< Lowest supported sample rate (Hz)
    double sampleRateMaxHz{0.0};///< Highest supported sample rate (Hz)
    double gainMinDb{0.0};      ///< Minimum overall gain (dB)
    double gainMaxDb{0.0};      ///< Maximum overall gain (dB)
    QStringList gainElements;   ///< Named gain stages the tuner exposes
    QStringList antennas;       ///< Available antenna ports
    bool hasAgc{false};         ///< Whether automatic gain control is available
};

/**
 * @struct SweepStatus
 * @purpose We emit this struct with sweep progress for UI updates
 */
struct SweepStatus {
    double currentFreqHz;       ///< We are currently tuned to this frequency
    uint32_t currentStep;       ///< Current step index within the sweep
    uint32_t totalSteps;        ///< Total steps in the current cycle
    uint32_t cycleCount;        ///< Number of complete sweep cycles performed
    double signalPowerDb;       ///< Measured signal power at current frequency (dBFS)
    DemodulationMode demodMode; ///< Active demodulation mode
    bool isRunning;             ///< Whether we are actively sweeping
};

/**
 * @class SweepEngine
 * @purpose We manage the SDR hardware lifecycle, frequency sweep execution, and IQ data streaming.
 *          The engine runs its sweep loop on a dedicated worker thread to avoid blocking the UI.
 */
class SweepEngine : public QObject {
    Q_OBJECT

public:
    explicit SweepEngine(QObject* parent = nullptr);
    ~SweepEngine() override;

    // ─── Device Management ─────────────────────────────────────────────────

    /**
     * @brief We enumerate all available SoapySDR devices on the system
     * @return Vector of discovered device info structs
     */
    static std::vector<SdrDeviceInfo> enumerateDevices();

    /**
     * @brief We open and initialize the specified SDR device
     * @param deviceInfo The device to open (from enumerateDevices)
     * @return true if we successfully opened and configured the device
     */
    bool openDevice(const SdrDeviceInfo& deviceInfo);

    /**
     * @brief We close the active SDR device and release resources
     */
    void closeDevice();

    /**
     * @brief We check if an SDR device is currently open and ready
     */
    [[nodiscard]] bool isDeviceOpen() const;

    /**
     * @brief We retrieve the currently opened device info
     */
    [[nodiscard]] SdrDeviceInfo currentDeviceInfo() const;

    /**
     * @brief We return the capabilities probed from the currently open device.
     *        Invalid (valid=false) when no device is open.
     */
    [[nodiscard]] SdrCapabilities capabilities() const;

    // ─── Sweep Control ─────────────────────────────────────────────────────

    /**
     * @brief We load and apply a sweep profile to configure the scan parameters
     * @param profile The sweep profile containing band and timing configuration
     */
    void setProfile(const SweepProfile& profile);

    /**
     * @brief We retrieve the currently active sweep profile
     */
    [[nodiscard]] SweepProfile currentProfile() const;

    /**
     * @brief We start the frequency sweep on the worker thread
     */
    void startSweep();

    /**
     * @brief We stop the frequency sweep gracefully
     */
    void stopSweep();

    /**
     * @brief We check if the sweep is currently running
     */
    [[nodiscard]] bool isSweeping() const;

    // ─── Real-time Parameters ──────────────────────────────────────────────

    /**
     * @brief We adjust the dwell time without stopping the sweep
     * @param ms New dwell time in milliseconds
     */
    void setDwellTimeMs(uint32_t ms);

    /**
     * @brief We adjust the gain without stopping the sweep
     * @param gain New gain in dB, or 0 for AGC
     */
    void setGain(double gain);

signals:
    /**
     * @brief We emit demodulated IQ samples for downstream audio processing.
     *        These are complex float samples at the SDR sample rate.
     * @param samples Vector of complex float IQ samples
     * @param freqHz The center frequency these samples were captured at
     * @param demodMode The demodulation mode for this frequency
     */
    void iqSamplesReady(const std::vector<std::complex<float>>& samples,
                        double freqHz, DemodulationMode demodMode);

    /**
     * @brief We emit sweep status updates for the UI at regular intervals
     */
    void sweepStatusUpdated(const SweepStatus& status);

    /**
     * @brief We emit when the sweep engine encounters an error
     */
    void errorOccurred(const QString& errorMessage);

    /**
     * @brief We emit when the device is opened or closed
     */
    void deviceStateChanged(bool isOpen);

    /**
     * @brief We emit the capabilities we probed right after opening a device.
     */
    void capabilitiesProbed(const SdrCapabilities& caps);

    /**
     * @brief We emit signal power measurements for the spectrum visualizer
     * @param freqHz Frequency in Hz
     * @param powerDb Signal power in dBFS
     */
    void signalPowerMeasured(double freqHz, double powerDb);

private slots:
    void onSweepThreadStarted();

private:
    /**
     * @brief We run the sweep loop on the worker thread. This method handles
     *        frequency hopping, IQ capture, and sample emission.
     */
    void sweepLoop();

    /**
     * @brief We tune the SDR to the specified frequency
     * @param freqHz Target frequency in Hz
     * @return true if the retune succeeded
     */
    bool tuneToFrequency(double freqHz);

    /**
     * @brief We read IQ samples from the SDR stream
     * @param buffer Output buffer for complex float samples
     * @param numSamples Number of samples to read
     * @return Number of samples actually read
     */
    int readSamples(std::vector<std::complex<float>>& buffer, size_t numSamples);

    /**
     * @brief We calculate signal power in dBFS from IQ samples
     */
    double calculatePowerDb(const std::vector<std::complex<float>>& samples) const;

    /**
     * @brief We build the linearized step list from the active profile's bands
     */
    void buildStepList();

    /**
     * @brief We probe the open device's real capabilities via SoapySDR.
     *        Caller must hold m_deviceMutex.
     */
    SdrCapabilities probeCapabilitiesLocked();

    // ─── Member Variables ──────────────────────────────────────────────────

    SoapySDR::Device* m_device{nullptr};
    SoapySDR::Stream* m_rxStream{nullptr};
    SdrDeviceInfo m_currentDevice;
    SdrCapabilities m_capabilities;
    SweepProfile m_profile;

    /**
     * @struct FrequencyStep
     * @purpose We store precomputed frequency steps for the sweep loop
     */
    struct FrequencyStep {
        double freqHz;
        DemodulationMode demodMode;
        double bandwidthHz;
    };

    std::vector<FrequencyStep> m_stepList;
    uint32_t m_currentStepIndex{0};
    uint32_t m_cycleCount{0};

    QThread* m_sweepThread{nullptr};
    std::atomic<bool> m_sweepRunning{false};
    std::atomic<uint32_t> m_dwellTimeMs{50};
    std::atomic<double> m_gain{0.0};
    QMutex m_deviceMutex;

    static constexpr size_t IQ_BUFFER_SIZE = 4096;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SWEEP_ENGINE_H
