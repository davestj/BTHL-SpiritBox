/**
 * @file src/ui/AudioWaveformWidget.cpp
 * @author David St John (davestj)
 * @date 2026-04-12
 * CHANGELOG:
 * 2026-03-30 - Stub
 * 2026-04-12 - Full QPainter waveform with VU meter and VAD confidence
 */
#include "ui/AudioWaveformWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace bthl::spiritbox {

AudioWaveformWidget::AudioWaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 120);
    setStyleSheet("background-color: #0a0a1a;");
    waveform_buffer_.resize(BUFFER_SIZE, 0.0f);
}

void AudioWaveformWidget::addAudioSamples(const std::vector<float>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (float s : samples) {
        waveform_buffer_.push_back(s);
    }
    while (waveform_buffer_.size() > BUFFER_SIZE) {
        waveform_buffer_.erase(waveform_buffer_.begin());
    }
    update();
}

void AudioWaveformWidget::setVADConfidence(float confidence) {
    vad_confidence_ = confidence;
    update();
}

void AudioWaveformWidget::setVADMetrics(float energy, float zcr,
                                         float flatness, float confidence) {
    vad_energy_ = energy;
    vad_zcr_ = zcr;
    vad_flatness_ = flatness;
    vad_confidence_ = confidence;
    update();
}

void AudioWaveformWidget::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(waveform_buffer_.begin(), waveform_buffer_.end(), 0.0f);
    vad_confidence_ = 0.0f;
    update();
}

void AudioWaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    p.fillRect(0, 0, w, h, QColor(10, 10, 26));

    /// We draw title
    p.setPen(QColor(180, 200, 200));
    p.setFont(QFont("Monospace", 10, QFont::Bold));
    p.drawText(8, 16, "Audio Waveform");

    int plotX = 8, plotY = 24, plotW = w - 16, plotH = h - 56;
    int cy = plotY + plotH / 2;

    /// We draw center line
    p.setPen(QPen(QColor(40, 40, 60), 1, Qt::DashLine));
    p.drawLine(plotX, cy, plotX + plotW, cy);

    /// We draw the waveform
    std::lock_guard<std::mutex> lock(mutex_);

    p.setPen(QPen(QColor(0, 200, 160), 1));
    size_t bufSize = waveform_buffer_.size();
    if (bufSize < 2) return;

    double step = static_cast<double>(bufSize) / plotW;
    for (int x = 1; x < plotW; ++x) {
        size_t idx0 = static_cast<size_t>((x - 1) * step);
        size_t idx1 = static_cast<size_t>(x * step);
        if (idx0 >= bufSize || idx1 >= bufSize) break;

        float s0 = waveform_buffer_[idx0];
        float s1 = waveform_buffer_[idx1];
        int y0 = cy - static_cast<int>(s0 * plotH / 2);
        int y1 = cy - static_cast<int>(s1 * plotH / 2);
        p.drawLine(plotX + x - 1, y0, plotX + x, y1);
    }

    /// We draw the VU meter bar at bottom
    int vuY = h - 28;
    int vuH = 12;

    /// We compute RMS for VU level
    float rms = 0.0f;
    for (float s : waveform_buffer_) rms += s * s;
    rms = std::sqrt(rms / bufSize);
    float vuNorm = std::clamp(rms * 10.0f, 0.0f, 1.0f);

    p.fillRect(plotX, vuY, plotW, vuH, QColor(20, 20, 30));
    QColor vuColor = (vuNorm > 0.8f) ? QColor(255, 60, 60) :
                     (vuNorm > 0.5f) ? QColor(255, 200, 0) :
                                       QColor(0, 200, 130);
    p.fillRect(plotX, vuY, static_cast<int>(plotW * vuNorm), vuH, vuColor);

    /// We draw VAD confidence bar below VU
    int vadY = h - 14;
    p.fillRect(plotX, vadY, plotW, 10, QColor(20, 20, 30));
    QColor vadColor = (vad_confidence_ > 0.7f) ? QColor(76, 175, 80) :
                      (vad_confidence_ > 0.3f) ? QColor(255, 193, 7) :
                                                  QColor(80, 80, 100);
    p.fillRect(plotX, vadY, static_cast<int>(plotW * vad_confidence_), 10, vadColor);

    /// We draw labels
    p.setPen(QColor(140, 160, 150));
    p.setFont(QFont("Monospace", 8));
    p.drawText(plotX + plotW + 2, vuY + 10, "VU");
    p.drawText(plotX + plotW + 2, vadY + 9, "VAD");

    /// We draw confidence value
    p.setPen(vad_confidence_ > 0.5f ? QColor(0, 220, 130) : QColor(100, 100, 120));
    p.drawText(w - 80, 16, QString("Conf: %1%").arg(
        static_cast<int>(vad_confidence_ * 100)));
}

} // namespace bthl::spiritbox
