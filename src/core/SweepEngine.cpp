/**
 * @file src/core/SweepEngine.cpp
 * @title SweepEngine Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement the SDR frequency sweep engine using SoapySDR for multi-device support.
 *          This handles device enumeration, stream setup, frequency hopping, and IQ sample capture.
 * @reason The sweep engine must run with precise timing control on a dedicated thread to ensure
 *         consistent dwell times and uninterrupted audio output.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with SoapySDR device lifecycle management
 */

#include "core/SweepEngine.h"
#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>
#include <QDebug>
#include <QTimer>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

namespace bthl::spiritbox {

SweepEngine::SweepEngine(QObject* parent)
    : QObject(parent) {
    qInfo() << "SweepEngine: We initialized with SoapySDR version" << SoapySDR::getAPIVersion().c_str();
}

SweepEngine::~SweepEngine() {
    stopSweep();
    closeDevice();
}

// ─── Device Management ─────────────────────────────────────────────────────────

std::vector<SdrDeviceInfo> SweepEngine::enumerateDevices() {
    std::vector<SdrDeviceInfo> devices;

    // We enumerate all available SoapySDR devices on the system
    auto results = SoapySDR::Device::enumerate();
    qInfo() << "SweepEngine: We found" << results.size() << "SoapySDR device(s)";

    for (const auto& kwargs : results) {
        SdrDeviceInfo info;
        info.kwargs = kwargs;

        // We extract human-readable device metadata
        if (kwargs.count("label")) {
            info.label = QString::fromStdString(kwargs.at("label"));
        } else if (kwargs.count("product")) {
            info.label = QString::fromStdString(kwargs.at("product"));
        } else {
            info.label = "Unknown SDR Device";
        }

        if (kwargs.count("driver")) {
            info.driver = QString::fromStdString(kwargs.at("driver"));
        }

        if (kwargs.count("serial")) {
            info.serial = QString::fromStdString(kwargs.at("serial"));
        }

        qInfo() << "SweepEngine: We discovered device:" << info.label
                << "driver:" << info.driver << "serial:" << info.serial;

        devices.push_back(std::move(info));
    }

    return devices;
}

bool SweepEngine::openDevice(const SdrDeviceInfo& deviceInfo) {
    QMutexLocker lock(&m_deviceMutex);

    // We close any existing device before opening a new one
    if (m_device) {
        qWarning() << "SweepEngine: We are closing existing device before opening new one";
        closeDevice();
    }

    try {
        // We create the SoapySDR device instance
        m_device = SoapySDR::Device::make(deviceInfo.kwargs);
        if (!m_device) {
            emit errorOccurred("We failed to create SoapySDR device instance");
            return false;
        }

        m_currentDevice = deviceInfo;

        // We configure the device with our profile settings
        m_device->setSampleRate(SOAPY_SDR_RX, 0, m_profile.sdrSampleRate());

        // We set the gain mode and value
        if (m_gain.load() == 0.0) {
            m_device->setGainMode(SOAPY_SDR_RX, 0, true);  // AGC
            qInfo() << "SweepEngine: We enabled automatic gain control";
        } else {
            m_device->setGainMode(SOAPY_SDR_RX, 0, false);
            m_device->setGain(SOAPY_SDR_RX, 0, m_gain.load());
            qInfo() << "SweepEngine: We set manual gain to" << m_gain.load() << "dB";
        }

        // We set up the receive stream with complex float format
        m_rxStream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
        if (!m_rxStream) {
            emit errorOccurred("We failed to set up RX stream");
            SoapySDR::Device::unmake(m_device);
            m_device = nullptr;
            return false;
        }

        // We probe what this physical device can actually do, so the UI and the sweep
        // planner work within real hardware limits (e.g. an E4000 cannot reach the AM band).
        m_capabilities = probeCapabilitiesLocked();

        // We rebuild the step list now that we know the real tunable range, so any
        // already-loaded profile immediately benefits from out-of-range auto-skip.
        buildStepList();

        qInfo() << "SweepEngine: We successfully opened device:" << deviceInfo.label;
        if (m_capabilities.valid) {
            qInfo() << "SweepEngine: Tuner" << m_capabilities.tuner
                    << "range" << m_capabilities.freqMinHz / 1e6 << "-"
                    << m_capabilities.freqMaxHz / 1e6 << "MHz";
        }
        emit deviceStateChanged(true);
        emit capabilitiesProbed(m_capabilities);
        return true;

    } catch (const std::exception& ex) {
        QString errorMsg = QString("We encountered an error opening device: %1").arg(ex.what());
        qCritical() << "SweepEngine:" << errorMsg;
        emit errorOccurred(errorMsg);

        if (m_device) {
            SoapySDR::Device::unmake(m_device);
            m_device = nullptr;
        }
        return false;
    }
}

void SweepEngine::closeDevice() {
    QMutexLocker lock(&m_deviceMutex);

    if (m_rxStream && m_device) {
        m_device->deactivateStream(m_rxStream);
        m_device->closeStream(m_rxStream);
        m_rxStream = nullptr;
        qInfo() << "SweepEngine: We closed the RX stream";
    }

    if (m_device) {
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        qInfo() << "SweepEngine: We released the SoapySDR device";
    }

    m_capabilities = SdrCapabilities{};  // We drop stale capabilities with the device

    emit deviceStateChanged(false);
}

bool SweepEngine::isDeviceOpen() const {
    return m_device != nullptr;
}

SdrDeviceInfo SweepEngine::currentDeviceInfo() const {
    return m_currentDevice;
}

SdrCapabilities SweepEngine::capabilities() const {
    return m_capabilities;
}

SdrCapabilities SweepEngine::probeCapabilitiesLocked() {
    SdrCapabilities caps;
    if (!m_device) return caps;

    try {
        caps.driver = QString::fromStdString(m_device->getDriverKey());
        caps.hardwareKey = QString::fromStdString(m_device->getHardwareKey());

        // We read the tuner chip from the hardware info if the driver reports it.
        const SoapySDR::Kwargs hw = m_device->getHardwareInfo();
        auto it = hw.find("tuner");
        if (it != hw.end()) caps.tuner = QString::fromStdString(it->second);

        // Full tunable frequency range (union of all sub-ranges the device reports).
        const auto freqRanges = m_device->getFrequencyRange(SOAPY_SDR_RX, 0);
        if (!freqRanges.empty()) {
            caps.freqMinHz = freqRanges.front().minimum();
            caps.freqMaxHz = freqRanges.back().maximum();
            for (const auto& r : freqRanges) {
                caps.freqMinHz = std::min(caps.freqMinHz, r.minimum());
                caps.freqMaxHz = std::max(caps.freqMaxHz, r.maximum());
            }
        }

        // Sample-rate envelope.
        const auto srRanges = m_device->getSampleRateRange(SOAPY_SDR_RX, 0);
        if (!srRanges.empty()) {
            caps.sampleRateMinHz = srRanges.front().minimum();
            caps.sampleRateMaxHz = srRanges.back().maximum();
            for (const auto& r : srRanges) {
                caps.sampleRateMinHz = std::min(caps.sampleRateMinHz, r.minimum());
                caps.sampleRateMaxHz = std::max(caps.sampleRateMaxHz, r.maximum());
            }
        }

        // Overall gain range and named gain stages.
        const SoapySDR::Range gainRange = m_device->getGainRange(SOAPY_SDR_RX, 0);
        caps.gainMinDb = gainRange.minimum();
        caps.gainMaxDb = gainRange.maximum();
        for (const auto& g : m_device->listGains(SOAPY_SDR_RX, 0)) {
            caps.gainElements << QString::fromStdString(g);
        }

        for (const auto& a : m_device->listAntennas(SOAPY_SDR_RX, 0)) {
            caps.antennas << QString::fromStdString(a);
        }

        caps.hasAgc = m_device->hasGainMode(SOAPY_SDR_RX, 0);
        caps.valid = true;
    } catch (const std::exception& ex) {
        qWarning() << "SweepEngine: We could not fully probe device capabilities:" << ex.what();
        // We keep whatever we gathered; valid stays false unless we completed above.
    }
    return caps;
}

// ─── Sweep Control ─────────────────────────────────────────────────────────────

void SweepEngine::setProfile(const SweepProfile& profile) {
    m_profile = profile;
    m_dwellTimeMs.store(profile.dwellTimeMs());
    m_gain.store(profile.gain());
    buildStepList();
    qInfo() << "SweepEngine: We loaded profile" << profile.name()
            << "with" << m_stepList.size() << "frequency steps";
}

SweepProfile SweepEngine::currentProfile() const {
    return m_profile;
}

void SweepEngine::startSweep() {
    if (m_sweepRunning.load()) {
        qWarning() << "SweepEngine: We are already sweeping, ignoring start request";
        return;
    }

    if (!m_device) {
        emit errorOccurred("We cannot start sweep without an open device");
        return;
    }

    if (m_stepList.empty()) {
        emit errorOccurred("We cannot start sweep with an empty step list");
        return;
    }

    // We activate the SDR receive stream
    {
        QMutexLocker lock(&m_deviceMutex);
        qInfo() << "SweepEngine: Activating RX stream...";
        int ret = m_device->activateStream(m_rxStream);
        if (ret != 0) {
            QString err = QString("We failed to activate RX stream: %1").arg(SoapySDR::errToStr(ret));
            qCritical() << "SweepEngine:" << err;
            emit errorOccurred(err);
            return;
        }
        qInfo() << "SweepEngine: RX stream activated";
    }

    m_currentStepIndex = 0;
    m_cycleCount = 0;
    m_sweepRunning.store(true);

    // We launch the sweep loop on a dedicated thread
    m_sweepThread = QThread::create([this]() { sweepLoop(); });
    m_sweepThread->setObjectName("SweepEngineThread");
    connect(m_sweepThread, &QThread::started, this, &SweepEngine::onSweepThreadStarted);
    connect(m_sweepThread, &QThread::finished, m_sweepThread, &QThread::deleteLater);
    m_sweepThread->start(QThread::TimeCriticalPriority);

    qInfo() << "SweepEngine: We started sweeping with dwell time" << m_dwellTimeMs.load() << "ms";
}

void SweepEngine::stopSweep() {
    if (!m_sweepRunning.load()) {
        return;
    }

    m_sweepRunning.store(false);

    // We wait for the sweep thread to finish
    if (m_sweepThread && m_sweepThread->isRunning()) {
        m_sweepThread->quit();
        m_sweepThread->wait(2000);
    }

    // We deactivate the SDR stream
    {
        QMutexLocker lock(&m_deviceMutex);
        if (m_device && m_rxStream) {
            m_device->deactivateStream(m_rxStream);
        }
    }

    qInfo() << "SweepEngine: We stopped sweeping after" << m_cycleCount << "complete cycles";
}

bool SweepEngine::isSweeping() const {
    return m_sweepRunning.load();
}

void SweepEngine::setDwellTimeMs(uint32_t ms) {
    m_dwellTimeMs.store(ms);
}

void SweepEngine::setGain(double gain) {
    m_gain.store(gain);

    QMutexLocker lock(&m_deviceMutex);
    if (m_device) {
        if (gain == 0.0) {
            m_device->setGainMode(SOAPY_SDR_RX, 0, true);
        } else {
            m_device->setGainMode(SOAPY_SDR_RX, 0, false);
            m_device->setGain(SOAPY_SDR_RX, 0, gain);
        }
    }
}

// ─── Private Implementation ────────────────────────────────────────────────────

void SweepEngine::onSweepThreadStarted() {
    qInfo() << "SweepEngine: We launched the sweep thread";
}

void SweepEngine::sweepLoop() {
    std::vector<std::complex<float>> iqBuffer(IQ_BUFFER_SIZE);
    QElapsedTimer dwellTimer;

    qInfo() << "SweepEngine: Sweep loop started with" << m_stepList.size() << "steps";

    while (m_sweepRunning.load()) {
        const auto& step = m_stepList[m_currentStepIndex];

        // We retune the SDR to the next frequency in our step list
        if (!tuneToFrequency(step.freqHz)) {
            // We skip this step if retune fails and move on
            qWarning() << "SweepEngine: We failed to tune to"
                       << step.freqHz / 1e6 << "MHz, skipping";
            m_currentStepIndex = (m_currentStepIndex + 1) % m_stepList.size();
            continue;
        }

        dwellTimer.restart();

        // We capture IQ samples for the duration of the dwell time
        while (dwellTimer.elapsed() < static_cast<qint64>(m_dwellTimeMs.load())
               && m_sweepRunning.load()) {

            int samplesRead = readSamples(iqBuffer, IQ_BUFFER_SIZE);
            if (samplesRead > 0) {
                // We resize the buffer to actual samples read for emission
                std::vector<std::complex<float>> emitBuffer(
                    iqBuffer.begin(), iqBuffer.begin() + samplesRead);

                // We emit the IQ samples for demodulation and audio output
                emit iqSamplesReady(emitBuffer, step.freqHz, step.demodMode);

                // We calculate and emit signal power for the spectrum display
                double powerDb = calculatePowerDb(emitBuffer);
                emit signalPowerMeasured(step.freqHz, powerDb);
            }
        }

        // We advance to the next step and check for cycle completion
        m_currentStepIndex++;
        if (m_currentStepIndex >= m_stepList.size()) {
            m_currentStepIndex = 0;
            m_cycleCount++;
        }

        // We emit status updates for the UI
        SweepStatus status;
        status.currentFreqHz = step.freqHz;
        status.currentStep = static_cast<uint32_t>(m_currentStepIndex);
        status.totalSteps = static_cast<uint32_t>(m_stepList.size());
        status.cycleCount = m_cycleCount;
        status.signalPowerDb = calculatePowerDb(iqBuffer);
        status.demodMode = step.demodMode;
        status.isRunning = m_sweepRunning.load();
        emit sweepStatusUpdated(status);
    }
}

bool SweepEngine::tuneToFrequency(double freqHz) {
    QMutexLocker lock(&m_deviceMutex);
    if (!m_device) return false;

    try {
        m_device->setFrequency(SOAPY_SDR_RX, 0, freqHz);
        return true;
    } catch (const std::exception& ex) {
        qWarning() << "SweepEngine: We caught exception during retune:" << ex.what();
        return false;
    }
}

int SweepEngine::readSamples(std::vector<std::complex<float>>& buffer, size_t numSamples) {
    QMutexLocker lock(&m_deviceMutex);
    if (!m_device || !m_rxStream) return 0;

    void* buffs[] = { buffer.data() };
    int flags = 0;
    long long timeNs = 0;

    int ret = m_device->readStream(m_rxStream, buffs,
                                    static_cast<unsigned int>(numSamples),
                                    flags, timeNs, 100000); // 100ms timeout

    if (ret < 0) {
        if (ret != SOAPY_SDR_TIMEOUT) {
            qWarning() << "SweepEngine: We encountered a stream read error:"
                       << SoapySDR::errToStr(ret);
        }
        return 0;
    }

    return ret;
}

double SweepEngine::calculatePowerDb(const std::vector<std::complex<float>>& samples) const {
    if (samples.empty()) return -120.0;

    // We calculate the RMS power of the IQ samples in dBFS
    double sumSquared = 0.0;
    for (const auto& s : samples) {
        sumSquared += static_cast<double>(s.real() * s.real() + s.imag() * s.imag());
    }
    double rms = std::sqrt(sumSquared / static_cast<double>(samples.size()));

    if (rms < 1e-12) return -120.0;

    return 20.0 * std::log10(rms);
}

void SweepEngine::buildStepList() {
    m_stepList.clear();

    // We honor the device's real tunable range when we know it, so we never waste the sweep
    // hammering frequencies the tuner physically cannot reach (e.g. AM on an E4000). We add a
    // small guard band inside the hardware limits to avoid edge-of-range PLL failures.
    const bool haveRange = m_capabilities.valid && m_capabilities.freqMaxHz > m_capabilities.freqMinHz;
    const double devMin = haveRange ? m_capabilities.freqMinHz : 0.0;
    const double devMax = haveRange ? m_capabilities.freqMaxHz : 0.0;

    size_t skipped = 0;
    for (const auto& band : m_profile.bands()) {
        double freq = band.startFreqHz;
        while (freq <= band.stopFreqHz) {
            if (haveRange && (freq < devMin || freq > devMax)) {
                ++skipped;            // Out of this tuner's reach — skip instead of failing to tune
            } else {
                FrequencyStep step;
                step.freqHz = freq;
                step.demodMode = band.demodMode;
                step.bandwidthHz = band.bandwidthHz;
                m_stepList.push_back(step);
            }
            freq += band.stepSizeHz;
        }
    }

    if (skipped > 0) {
        qInfo() << "SweepEngine: We skipped" << skipped
                << "out-of-range steps (tuner reaches" << devMin / 1e6 << "-"
                << devMax / 1e6 << "MHz)";
        emit errorOccurred(
            QString("%1 frequencies are outside this tuner's range (%2–%3 MHz) and were skipped.")
                .arg(skipped)
                .arg(devMin / 1e6, 0, 'f', 3)
                .arg(devMax / 1e6, 0, 'f', 1));
    }

    // We randomize the step order if the profile requests it
    if (m_profile.isRandomized() && !m_stepList.empty()) {
        auto rng = std::default_random_engine(
            static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::shuffle(m_stepList.begin(), m_stepList.end(), rng);
        qInfo() << "SweepEngine: We randomized" << m_stepList.size() << "frequency steps";
    }

    qInfo() << "SweepEngine: We built step list with" << m_stepList.size() << "entries";
}

} // namespace bthl::spiritbox
