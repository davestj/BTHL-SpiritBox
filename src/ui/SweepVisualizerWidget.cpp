/**
 * @file src/ui/SweepVisualizerWidget.cpp
 * @author David St John (davestj)
 * @date 2026-04-12
 * CHANGELOG:
 * 2026-03-30 - Stub
 * 2026-04-12 - Full QPainter waterfall implementation
 */
#include "ui/SweepVisualizerWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace bthl::spiritbox {

SweepVisualizerWidget::SweepVisualizerWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 200);
    setStyleSheet("background-color: #0a0a1a;");
}

void SweepVisualizerWidget::addSweepData(double, double powerDb,
                                          uint32_t /*step*/, uint32_t totalSteps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (expected_steps_ != totalSteps) {
        expected_steps_ = totalSteps;
        current_row_.clear();
    }
    current_row_.push_back(powerDb);

    if (current_row_.size() >= expected_steps_ && expected_steps_ > 0) {
        waterfall_rows_.push_front(current_row_);
        current_row_.clear();
        while (waterfall_rows_.size() > MAX_ROWS) waterfall_rows_.pop_back();
    }
    update();
}

void SweepVisualizerWidget::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    waterfall_rows_.clear();
    current_row_.clear();
    update();
}

void SweepVisualizerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    int w = width(), h = height();
    p.fillRect(0, 0, w, h, QColor(10, 10, 26));

    std::lock_guard<std::mutex> lock(mutex_);
    if (waterfall_rows_.empty()) {
        p.setPen(QColor(80, 80, 100));
        p.setFont(QFont("Monospace", 11));
        p.drawText(rect(), Qt::AlignCenter, "Waiting for sweep data...");
        return;
    }

    p.setPen(QColor(180, 200, 200));
    p.setFont(QFont("Monospace", 10, QFont::Bold));
    p.drawText(8, 16, "Spectrum Waterfall");

    int plotY = 24, plotH = h - 28;
    size_t numRows = waterfall_rows_.size();
    double rh = std::max(1.0, static_cast<double>(plotH) / numRows);

    for (size_t r = 0; r < numRows; ++r) {
        const auto& row = waterfall_rows_[r];
        int y = plotY + static_cast<int>(r * rh);
        if (y >= h) break;
        for (size_t s = 0; s < row.size(); ++s) {
            int x = static_cast<int>(static_cast<double>(w) * s / row.size());
            int cw = std::max(1, w / static_cast<int>(row.size()));
            p.fillRect(x, y, cw, std::max(1, static_cast<int>(rh)), powerToColor(row[s]));
        }
    }
}

QColor SweepVisualizerWidget::powerToColor(double powerDb) const {
    double range = max_power_db_ - min_power_db_;
    if (range < 1.0) range = 60.0;
    double norm = std::clamp((powerDb - min_power_db_) / range, 0.0, 1.0);

    /// We use the BTHL cosmic gradient: deep navy → teal → gold → white
    int r = static_cast<int>(norm < 0.5 ? norm * 2 * 40 : 40 + (norm - 0.5) * 2 * 215);
    int g = static_cast<int>(norm < 0.5 ? 10 + norm * 2 * 170 : 180 + (norm - 0.5) * 2 * 75);
    int b = static_cast<int>(norm < 0.5 ? 30 + norm * 2 * 100 : 130 - (norm - 0.5) * 2 * 80);
    return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

} // namespace bthl::spiritbox
