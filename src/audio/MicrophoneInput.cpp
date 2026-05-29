/**
 * @file src/audio/MicrophoneInput.cpp
 * @title MicrophoneInput - Investigator Microphone Capture (implementation)
 * @author David St John (davestj)
 * @date 2026-05-28
 * @purpose We implement format-agnostic microphone capture that delivers 16 kHz mono
 *          float audio to the investigator voice-detection path.
 *
 * CHANGELOG:
 * 2026-05-28 - Initial implementation.
 */

#include "audio/MicrophoneInput.h"

#include <QAudioSource>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QIODevice>
#include <QDebug>

#include <cstring>

namespace bthl::spiritbox {

MicrophoneInput::MicrophoneInput(QObject* parent) : QObject(parent) {}

MicCapabilities MicrophoneInput::probeDefaultDevice() {
    MicCapabilities caps;
    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) return caps;

    caps.valid = true;
    caps.deviceName = device.description();
    caps.preferredSampleRate = device.preferredFormat().sampleRate();
    caps.minSampleRate = device.minimumSampleRate();
    caps.maxSampleRate = device.maximumSampleRate();
    caps.minChannels = device.minimumChannelCount();
    caps.maxChannels = device.maximumChannelCount();

    for (QAudioFormat::SampleFormat fmt : device.supportedSampleFormats()) {
        switch (fmt) {
            case QAudioFormat::UInt8: caps.sampleFormats << "UInt8"; break;
            case QAudioFormat::Int16: caps.sampleFormats << "Int16"; break;
            case QAudioFormat::Int32: caps.sampleFormats << "Int32"; break;
            case QAudioFormat::Float: caps.sampleFormats << "Float"; break;
            default: break;
        }
    }
    return caps;
}

MicrophoneInput::~MicrophoneInput() {
    stop();
}

bool MicrophoneInput::start() {
    if (m_running) return true;

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        emit errorOccurred("No microphone / audio input device is available.");
        qWarning() << "MicrophoneInput: We found no default audio input device";
        return false;
    }
    m_deviceName = device.description();

    // We prefer 16 kHz mono float so no conversion is needed; if the device cannot
    // provide it, we fall back to its preferred format and convert ourselves.
    QAudioFormat desired;
    desired.setSampleRate(static_cast<int>(kTargetSampleRate));
    desired.setChannelCount(1);
    desired.setSampleFormat(QAudioFormat::Float);

    m_format = device.isFormatSupported(desired) ? desired : device.preferredFormat();

    qInfo() << "MicrophoneInput: We opened" << m_deviceName
            << "at" << m_format.sampleRate() << "Hz"
            << m_format.channelCount() << "ch"
            << "fmt" << static_cast<int>(m_format.sampleFormat());

    m_source = new QAudioSource(device, m_format, this);
    m_resampleAcc.clear();
    m_resamplePos = 0.0;

    m_io = m_source->start();  // Pull mode — m_io is owned by m_source
    if (!m_io) {
        emit errorOccurred("We failed to start the microphone audio stream.");
        qWarning() << "MicrophoneInput: We failed to start QAudioSource";
        delete m_source;
        m_source = nullptr;
        return false;
    }

    connect(m_io, &QIODevice::readyRead, this, &MicrophoneInput::onReadyRead);
    m_running = true;
    return true;
}

void MicrophoneInput::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_io) {
        disconnect(m_io, &QIODevice::readyRead, this, &MicrophoneInput::onReadyRead);
        m_io = nullptr;  // Owned by m_source
    }
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_resampleAcc.clear();
    m_resamplePos = 0.0;
    qInfo() << "MicrophoneInput: We stopped microphone capture";
}

void MicrophoneInput::onReadyRead() {
    if (!m_io) return;

    const QByteArray chunk = m_io->readAll();
    if (chunk.isEmpty()) return;

    std::vector<float> monoNative = toMonoFloat(chunk.constData(), chunk.size());
    if (monoNative.empty()) return;

    std::vector<float> out = resampleTo16k(monoNative);
    if (!out.empty()) {
        emit audioReady(out, kTargetSampleRate);
    }
}

std::vector<float> MicrophoneInput::toMonoFloat(const char* data, qint64 bytes) const {
    const int channels = qMax(1, m_format.channelCount());
    const int bytesPerSample = m_format.bytesPerSample();
    if (bytesPerSample <= 0) return {};

    const qint64 totalSamples = bytes / bytesPerSample;       // across all channels
    const qint64 frames = totalSamples / channels;
    std::vector<float> mono;
    mono.reserve(static_cast<size_t>(frames));

    // We read interleaved samples, decode each to float in [-1, 1], and average channels.
    const auto sampleAt = [&](qint64 sampleIndex) -> float {
        const char* p = data + sampleIndex * bytesPerSample;
        switch (m_format.sampleFormat()) {
            case QAudioFormat::UInt8: {
                quint8 v;
                std::memcpy(&v, p, 1);
                return (static_cast<float>(v) - 128.0f) / 128.0f;
            }
            case QAudioFormat::Int16: {
                qint16 v;
                std::memcpy(&v, p, 2);
                return static_cast<float>(v) / 32768.0f;
            }
            case QAudioFormat::Int32: {
                qint32 v;
                std::memcpy(&v, p, 4);
                return static_cast<float>(v) / 2147483648.0f;
            }
            case QAudioFormat::Float: {
                float v;
                std::memcpy(&v, p, 4);
                return v;
            }
            default:
                return 0.0f;
        }
    };

    for (qint64 f = 0; f < frames; ++f) {
        float acc = 0.0f;
        for (int c = 0; c < channels; ++c) {
            acc += sampleAt(f * channels + c);
        }
        mono.push_back(acc / static_cast<float>(channels));
    }
    return mono;
}

std::vector<float> MicrophoneInput::resampleTo16k(const std::vector<float>& monoNative) {
    const double srcRate = static_cast<double>(m_format.sampleRate());
    std::vector<float> out;

    // Fast path — already at target rate, nothing to resample.
    if (srcRate == static_cast<double>(kTargetSampleRate)) {
        return monoNative;
    }
    if (srcRate <= 0.0) return monoNative;

    // We append the new native-rate samples to whatever tail we carried over, then walk
    // a fractional read cursor at the source/target ratio, linearly interpolating. We keep
    // the unconsumed tail and fractional phase so chunk boundaries stay seamless.
    m_resampleAcc.insert(m_resampleAcc.end(), monoNative.begin(), monoNative.end());

    const double ratio = srcRate / static_cast<double>(kTargetSampleRate);
    double pos = m_resamplePos;
    const size_t n = m_resampleAcc.size();

    while (pos + 1.0 < static_cast<double>(n)) {
        const size_t i = static_cast<size_t>(pos);
        const double frac = pos - static_cast<double>(i);
        out.push_back(m_resampleAcc[i] * (1.0f - static_cast<float>(frac)) +
                      m_resampleAcc[i + 1] * static_cast<float>(frac));
        pos += ratio;
    }

    // We drop the samples we fully consumed and rebase the fractional cursor.
    const size_t consumed = static_cast<size_t>(pos);
    if (consumed > 0 && consumed <= m_resampleAcc.size()) {
        m_resampleAcc.erase(m_resampleAcc.begin(), m_resampleAcc.begin() + consumed);
        pos -= static_cast<double>(consumed);
    }
    m_resamplePos = pos;

    return out;
}

} // namespace bthl::spiritbox
