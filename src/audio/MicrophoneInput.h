/**
 * @file src/audio/MicrophoneInput.h
 * @title MicrophoneInput - Investigator Microphone Capture
 * @author David St John (davestj)
 * @date 2026-05-28
 * @purpose We capture the investigator's microphone so their spoken questions and
 *          interactions are transcribed alongside the radio-band responses. We deliver
 *          audio as 16 kHz mono float — exactly what Whisper and the VAD expect.
 * @reason A spirit-box session is a conversation: the investigator asks a question out
 *         loud and listens for a response in the swept audio. Capturing the question side
 *         lets us build a complete, time-aligned Q&A transcript.
 *
 * CHANGELOG:
 * 2026-05-28 - Initial implementation using Qt6 QAudioSource with format-agnostic
 *              downmix + linear resample to 16 kHz mono.
 */

#ifndef BTHL_SPIRITBOX_MICROPHONE_INPUT_H
#define BTHL_SPIRITBOX_MICROPHONE_INPUT_H

#include <QObject>
#include <QAudioFormat>
#include <QString>
#include <vector>
#include <cstdint>

QT_BEGIN_NAMESPACE
class QAudioSource;
class QIODevice;
QT_END_NAMESPACE

namespace bthl::spiritbox {

/**
 * @struct MicCapabilities
 * @purpose We capture what the default microphone can actually do, probed from the OS audio
 *          device, so we can report it and pick a sane capture format.
 */
struct MicCapabilities {
    bool valid{false};
    QString deviceName;
    int preferredSampleRate{0};
    int minSampleRate{0};
    int maxSampleRate{0};
    int minChannels{0};
    int maxChannels{0};
    QStringList sampleFormats;   ///< Supported sample formats (e.g. "Int16", "Float")
};

/**
 * @class MicrophoneInput
 * @purpose We open the default system microphone and stream its audio as 16 kHz mono
 *          float buffers, regardless of the device's native sample rate, channel count,
 *          or sample format.
 */
class MicrophoneInput : public QObject {
    Q_OBJECT

public:
    /// We target 16 kHz mono float — the canonical Whisper input format.
    static constexpr uint32_t kTargetSampleRate = 16000;

    explicit MicrophoneInput(QObject* parent = nullptr);
    ~MicrophoneInput() override;

    /**
     * @brief We probe the default system microphone's real capabilities (no capture started).
     *        Returns valid=false if there is no input device.
     */
    static MicCapabilities probeDefaultDevice();

    /**
     * @brief We open the default input device and begin streaming.
     * @return true if the microphone opened successfully
     */
    bool start();

    /**
     * @brief We stop streaming and release the microphone.
     */
    void stop();

    [[nodiscard]] bool isRunning() const { return m_running; }

    /**
     * @brief We report the human-readable name of the active capture device.
     */
    [[nodiscard]] QString deviceName() const { return m_deviceName; }

signals:
    /**
     * @brief We emit captured audio as 16 kHz mono float, ready for the VAD/Whisper path.
     */
    void audioReady(const std::vector<float>& samples, uint32_t sampleRate);

    /**
     * @brief We emit when the microphone could not be opened or fails mid-stream.
     */
    void errorOccurred(const QString& message);

private slots:
    void onReadyRead();

private:
    /// We convert one device-format chunk to mono float at the device's native rate.
    std::vector<float> toMonoFloat(const char* data, qint64 bytes) const;

    /// We resample mono float from the device rate to 16 kHz, carrying state across chunks.
    std::vector<float> resampleTo16k(const std::vector<float>& monoNative);

    QAudioSource* m_source{nullptr};
    QIODevice* m_io{nullptr};   ///< Not owned — owned by m_source
    QAudioFormat m_format;
    QString m_deviceName;
    bool m_running{false};

    // ─── Stateful linear-resampler carry-over ──────────────────────────────
    std::vector<float> m_resampleAcc;  ///< Unconsumed native-rate samples
    double m_resamplePos{0.0};         ///< Fractional read position into m_resampleAcc
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_MICROPHONE_INPUT_H
