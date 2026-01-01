/**
 * @file src/core/SweepProfile.cpp
 * @title SweepProfile Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose Implementation of sweep profile configuration, serialization, and factory presets.
 * @reason We need reusable, saveable scan configurations for different paranormal investigation scenarios.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with AM, FM, VHF, and full spectrum presets
 */

#include "core/SweepProfile.h"
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <numeric>

namespace bthl::spiritbox {

// ─── SweepBand Implementation ──────────────────────────────────────────────────

uint32_t SweepBand::stepCount() const {
    if (stepSizeHz <= 0.0 || stopFreqHz <= startFreqHz) {
        return 0;
    }
    return static_cast<uint32_t>((stopFreqHz - startFreqHz) / stepSizeHz) + 1;
}

QJsonObject SweepBand::toJson() const {
    QJsonObject obj;
    obj["start_freq_hz"] = startFreqHz;
    obj["stop_freq_hz"] = stopFreqHz;
    obj["step_size_hz"] = stepSizeHz;
    obj["demod_mode"] = static_cast<int>(demodMode);
    obj["bandwidth_hz"] = bandwidthHz;
    return obj;
}

SweepBand SweepBand::fromJson(const QJsonObject& json) {
    SweepBand band;
    band.startFreqHz = json["start_freq_hz"].toDouble();
    band.stopFreqHz = json["stop_freq_hz"].toDouble();
    band.stepSizeHz = json["step_size_hz"].toDouble();
    band.demodMode = static_cast<DemodulationMode>(json["demod_mode"].toInt());
    band.bandwidthHz = json["bandwidth_hz"].toDouble();
    return band;
}

// ─── SweepProfile Implementation ───────────────────────────────────────────────

SweepProfile::SweepProfile(const QString& name)
    : m_name(name) {}

void SweepProfile::setName(const QString& name) { m_name = name; }
QString SweepProfile::name() const { return m_name; }

void SweepProfile::setDescription(const QString& desc) { m_description = desc; }
QString SweepProfile::description() const { return m_description; }

void SweepProfile::setDwellTimeMs(uint32_t ms) { m_dwellTimeMs = ms; }
uint32_t SweepProfile::dwellTimeMs() const { return m_dwellTimeMs; }

void SweepProfile::setAudioSampleRate(uint32_t rate) { m_audioSampleRate = rate; }
uint32_t SweepProfile::audioSampleRate() const { return m_audioSampleRate; }

void SweepProfile::setSdrSampleRate(double rate) { m_sdrSampleRate = rate; }
double SweepProfile::sdrSampleRate() const { return m_sdrSampleRate; }

void SweepProfile::setGain(double gain) { m_gain = gain; }
double SweepProfile::gain() const { return m_gain; }

void SweepProfile::addBand(const SweepBand& band) {
    m_bands.push_back(band);
}

void SweepProfile::removeBand(size_t index) {
    if (index < m_bands.size()) {
        m_bands.erase(m_bands.begin() + static_cast<ptrdiff_t>(index));
    }
}

void SweepProfile::clearBands() { m_bands.clear(); }
const std::vector<SweepBand>& SweepProfile::bands() const { return m_bands; }
size_t SweepProfile::bandCount() const { return m_bands.size(); }

uint32_t SweepProfile::totalSteps() const {
    return std::accumulate(m_bands.begin(), m_bands.end(), uint32_t{0},
        [](uint32_t sum, const SweepBand& band) {
            return sum + band.stepCount();
        });
}

uint32_t SweepProfile::estimatedCycleTimeMs() const {
    return totalSteps() * m_dwellTimeMs;
}

void SweepProfile::setBidirectional(bool enabled) { m_bidirectional = enabled; }
bool SweepProfile::isBidirectional() const { return m_bidirectional; }

void SweepProfile::setRandomized(bool enabled) { m_randomized = enabled; }
bool SweepProfile::isRandomized() const { return m_randomized; }

// ─── Serialization ─────────────────────────────────────────────────────────────

QJsonObject SweepProfile::toJson() const {
    QJsonObject obj;
    obj["name"] = m_name;
    obj["description"] = m_description;
    obj["dwell_time_ms"] = static_cast<int>(m_dwellTimeMs);
    obj["audio_sample_rate"] = static_cast<int>(m_audioSampleRate);
    obj["sdr_sample_rate"] = m_sdrSampleRate;
    obj["gain"] = m_gain;
    obj["bidirectional"] = m_bidirectional;
    obj["randomized"] = m_randomized;

    QJsonArray bandsArray;
    for (const auto& band : m_bands) {
        bandsArray.append(band.toJson());
    }
    obj["bands"] = bandsArray;

    return obj;
}

SweepProfile SweepProfile::fromJson(const QJsonObject& json) {
    SweepProfile profile;
    profile.m_name = json["name"].toString("Untitled");
    profile.m_description = json["description"].toString();
    profile.m_dwellTimeMs = static_cast<uint32_t>(json["dwell_time_ms"].toInt(50));
    profile.m_audioSampleRate = static_cast<uint32_t>(json["audio_sample_rate"].toInt(16000));
    profile.m_sdrSampleRate = json["sdr_sample_rate"].toDouble(2.4e6);
    profile.m_gain = json["gain"].toDouble(0.0);
    profile.m_bidirectional = json["bidirectional"].toBool(false);
    profile.m_randomized = json["randomized"].toBool(false);

    const QJsonArray bandsArray = json["bands"].toArray();
    for (const auto& bandVal : bandsArray) {
        profile.m_bands.push_back(SweepBand::fromJson(bandVal.toObject()));
    }

    return profile;
}

bool SweepProfile::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "SweepProfile: We could not open file for writing:" << filePath;
        return false;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    qInfo() << "SweepProfile: We saved profile" << m_name << "to" << filePath;
    return true;
}

SweepProfile SweepProfile::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "SweepProfile: We could not open file for reading:" << filePath;
        return SweepProfile{};
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return fromJson(doc.object());
}

// ─── Factory Presets ───────────────────────────────────────────────────────────

SweepProfile SweepProfile::createAMBroadcast() {
    SweepProfile profile("AM Broadcast");
    profile.setDescription("We scan the AM broadcast band (530 kHz - 1700 kHz) with 10 kHz steps");
    profile.setDwellTimeMs(50);
    profile.setAudioSampleRate(16000);
    profile.setSdrSampleRate(2.4e6);
    profile.setGain(0.0);

    SweepBand amBand;
    amBand.startFreqHz = 530000.0;      // 530 kHz
    amBand.stopFreqHz = 1700000.0;      // 1700 kHz
    amBand.stepSizeHz = 10000.0;        // 10 kHz steps (standard AM channel spacing)
    amBand.demodMode = DemodulationMode::AM;
    amBand.bandwidthHz = 10000.0;       // 10 kHz AM bandwidth
    profile.addBand(amBand);

    return profile;
}

SweepProfile SweepProfile::createFMBroadcast() {
    SweepProfile profile("FM Broadcast");
    profile.setDescription("We scan the FM broadcast band (88 MHz - 108 MHz) with 200 kHz steps");
    profile.setDwellTimeMs(30);
    profile.setAudioSampleRate(48000);
    profile.setSdrSampleRate(2.4e6);
    profile.setGain(0.0);

    SweepBand fmBand;
    fmBand.startFreqHz = 88000000.0;    // 88 MHz
    fmBand.stopFreqHz = 108000000.0;    // 108 MHz
    fmBand.stepSizeHz = 200000.0;       // 200 kHz steps (standard FM channel spacing)
    fmBand.demodMode = DemodulationMode::WFM;
    fmBand.bandwidthHz = 200000.0;      // 200 kHz FM bandwidth
    profile.addBand(fmBand);

    return profile;
}

SweepProfile SweepProfile::createVHFLow() {
    SweepProfile profile("VHF Low Band");
    profile.setDescription("We scan VHF low band (30 MHz - 88 MHz) for non-broadcast signals");
    profile.setDwellTimeMs(40);
    profile.setAudioSampleRate(16000);
    profile.setSdrSampleRate(2.4e6);
    profile.setGain(0.0);

    SweepBand vhfBand;
    vhfBand.startFreqHz = 30000000.0;   // 30 MHz
    vhfBand.stopFreqHz = 88000000.0;    // 88 MHz
    vhfBand.stepSizeHz = 25000.0;       // 25 kHz steps
    vhfBand.demodMode = DemodulationMode::NFM;
    vhfBand.bandwidthHz = 12500.0;      // 12.5 kHz NFM bandwidth
    profile.addBand(vhfBand);

    return profile;
}

SweepProfile SweepProfile::createFullSpectrum() {
    SweepProfile profile("Full Spectrum");
    profile.setDescription("We scan AM, FM, and VHF bands sequentially for maximum coverage");
    profile.setDwellTimeMs(25);
    profile.setAudioSampleRate(16000);
    profile.setSdrSampleRate(2.4e6);
    profile.setGain(0.0);

    // We add AM broadcast band
    SweepBand amBand;
    amBand.startFreqHz = 530000.0;
    amBand.stopFreqHz = 1700000.0;
    amBand.stepSizeHz = 10000.0;
    amBand.demodMode = DemodulationMode::AM;
    amBand.bandwidthHz = 10000.0;
    profile.addBand(amBand);

    // We add VHF low band
    SweepBand vhfBand;
    vhfBand.startFreqHz = 30000000.0;
    vhfBand.stopFreqHz = 88000000.0;
    vhfBand.stepSizeHz = 50000.0;
    vhfBand.demodMode = DemodulationMode::NFM;
    vhfBand.bandwidthHz = 12500.0;
    profile.addBand(vhfBand);

    // We add FM broadcast band
    SweepBand fmBand;
    fmBand.startFreqHz = 88000000.0;
    fmBand.stopFreqHz = 108000000.0;
    fmBand.stepSizeHz = 200000.0;
    fmBand.demodMode = DemodulationMode::WFM;
    fmBand.bandwidthHz = 200000.0;
    profile.addBand(fmBand);

    return profile;
}

} // namespace bthl::spiritbox
