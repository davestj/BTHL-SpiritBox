/**
 * @file src/ui/AudioWaveformWidget.h
 * @title AudioWaveformWidget - Real-time Audio Waveform Display
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We display real-time demodulated audio waveform with VU meter
 *          and voice detection confidence indicator.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial stub
 * 2026-04-12 - Full QPainter waveform implementation
 */

#ifndef BTHL_SPIRITBOX_AUDIO_WAVEFORM_WIDGET_H
#define BTHL_SPIRITBOX_AUDIO_WAVEFORM_WIDGET_H

#include <QWidget>
#include <mutex>
#include <vector>

namespace bthl::spiritbox {

class AudioWaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioWaveformWidget(QWidget* parent = nullptr);

public slots:
    void addAudioSamples(const std::vector<float>& samples);
    void setVADConfidence(float confidence);
    void setVADMetrics(float energy, float zcr, float flatness, float confidence);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<float> waveform_buffer_;
    static constexpr size_t BUFFER_SIZE = 4096;
    float vad_confidence_{0.0f};
    float vad_energy_{0.0f};
    float vad_zcr_{0.0f};
    float vad_flatness_{0.0f};
    std::mutex mutex_;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_AUDIO_WAVEFORM_WIDGET_H
