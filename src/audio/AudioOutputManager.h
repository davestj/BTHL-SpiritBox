/**
 * @file src/audio/AudioOutputManager.h
 * @title AudioOutputManager - Real-time Audio Playback via PortAudio
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We manage real-time audio output using PortAudio, feeding demodulated audio
 *          from the ring buffer to the system's audio output device.
 * @reason PortAudio gives us low-latency cross-platform audio output with callback-driven
 *         delivery, which is essential for real-time spirit box monitoring.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with PortAudio callback architecture
 */

#ifndef BTHL_SPIRITBOX_AUDIO_OUTPUT_MANAGER_H
#define BTHL_SPIRITBOX_AUDIO_OUTPUT_MANAGER_H

#include "audio/AudioRingBuffer.h"
#include <QObject>
#include <portaudio.h>
#include <memory>
#include <atomic>

namespace bthl::spiritbox {

/**
 * @class AudioOutputManager
 * @purpose We handle PortAudio initialization, stream management, and callback-driven
 *          audio playback from the ring buffer.
 */
class AudioOutputManager : public QObject {
    Q_OBJECT

public:
    explicit AudioOutputManager(QObject* parent = nullptr);
    ~AudioOutputManager() override;

    /**
     * @brief We initialize PortAudio and prepare for audio output
     * @param sampleRate Audio sample rate in Hz
     * @param framesPerBuffer Callback buffer size in frames
     * @return true if we successfully initialized
     */
    bool initialize(uint32_t sampleRate, uint32_t framesPerBuffer = 256);

    /**
     * @brief We start audio playback
     */
    bool start();

    /**
     * @brief We stop audio playback
     */
    void stop();

    /**
     * @brief We shut down PortAudio completely
     */
    void shutdown();

    /**
     * @brief We check if audio output is currently active
     */
    [[nodiscard]] bool isPlaying() const;

    /**
     * @brief We provide access to the ring buffer for the demodulator to write into
     */
    AudioRingBuffer& ringBuffer();

    /**
     * @brief We set the output volume (0.0 to 1.0)
     */
    void setVolume(float volume);

    /**
     * @brief We get the current output volume
     */
    [[nodiscard]] float volume() const;

    /**
     * @brief We mute or unmute the output
     */
    void setMuted(bool muted);
    [[nodiscard]] bool isMuted() const;

signals:
    void playbackStarted();
    void playbackStopped();
    void errorOccurred(const QString& message);

    /**
     * @brief We emit audio level for the VU meter display
     * @param level Peak audio level (0.0 to 1.0)
     */
    void audioLevelUpdated(float level);

private:
    /**
     * @brief We implement the PortAudio callback as a static function
     *        that reads from our ring buffer and writes to the output device
     */
    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);

    PaStream* m_stream{nullptr};
    AudioRingBuffer m_ringBuffer;
    std::atomic<float> m_volume{0.8f};
    std::atomic<bool> m_muted{false};
    std::atomic<bool> m_playing{false};
    uint32_t m_sampleRate{16000};
    bool m_initialized{false};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_AUDIO_OUTPUT_MANAGER_H
