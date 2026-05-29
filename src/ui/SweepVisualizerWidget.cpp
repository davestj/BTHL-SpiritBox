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
    if (waterfall_rows_.empty() && current_row_.empty()) {
        p.setPen(QColor(80, 80, 100));
        p.setFont(QFont("Monospace", 11));
        p.drawText(rect(), Qt::AlignCenter, "Waiting for sweep data...");
        return;
    }

    p.setPen(QColor(180, 200, 200));
    p.setFont(QFont("Monospace", 10, QFont::Bold));
    p.drawText(8, 16, "Spectrum Waterfall");

    int plotY = 24, plotH = h - 28;

    // We always show the in-progress sweep as the top row so the display updates live (one
    // cell per frequency step) instead of only refreshing once a full sweep completes. We then
    // scroll the completed rows beneath it.
    const bool hasPartial = !current_row_.empty();
    size_t numRows = waterfall_rows_.size() + (hasPartial ? 1 : 0);
    if (numRows == 0) return;
    double rh = std::max(1.0, static_cast<double>(plotH) / static_cast<double>(numRows));

    // We size cells against the expected steps-per-sweep so the partial row stays aligned
    // with completed rows as it fills.
    const size_t cols = expected_steps_ > 0
        ? expected_steps_
        : std::max<size_t>(current_row_.size(), 1);

    auto drawRow = [&](const std::vector<double>& row, int y) {
        if (y >= h) return;
        const int cellH = std::max(1, static_cast<int>(rh));
        for (size_t s = 0; s < row.size(); ++s) {
            int x = static_cast<int>(static_cast<double>(w) * static_cast<double>(s) / cols);
            int cw = std::max(1, static_cast<int>(static_cast<double>(w) / cols) + 1);
            p.fillRect(x, y, cw, cellH, powerToColor(row[s]));
        }
    };

    size_t r = 0;
    if (hasPartial) {
        drawRow(current_row_, plotY);  // newest, still-filling sweep at the top
        r = 1;
    }
    for (size_t i = 0; i < waterfall_rows_.size(); ++i, ++r) {
        int y = plotY + static_cast<int>(r * rh);
        if (y >= h) break;
        drawRow(waterfall_rows_[i], y);
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
