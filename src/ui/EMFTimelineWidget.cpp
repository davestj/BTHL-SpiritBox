/**
 * @file src/ui/EMFTimelineWidget.cpp
 * @author David St John (davestj)
 * @date 2026-04-12
 * CHANGELOG:
 * 2026-03-30 - Stub
 * 2026-04-12 - Full QPainter EMF timeline with spike markers
 */
#include "ui/EMFTimelineWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace bthl::spiritbox {

EMFTimelineWidget::EMFTimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 150);
    setStyleSheet("background-color: #0a0a1a;");
}

void EMFTimelineWidget::addReading(double timestamp, double emfMilligauss, bool isSpike) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.push_back({timestamp, emfMilligauss, isSpike});
    while (data_.size() > MAX_POINTS) data_.pop_front();
    if (emfMilligauss > max_emf_) max_emf_ = emfMilligauss * 1.2;
    update();
}

void EMFTimelineWidget::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    max_emf_ = 10.0;
    update();
}

void EMFTimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    p.fillRect(0, 0, w, h, QColor(10, 10, 26));

    std::lock_guard<std::mutex> lock(mutex_);

    /// We draw title and axis labels
    p.setPen(QColor(180, 200, 200));
    p.setFont(QFont("Monospace", 10, QFont::Bold));
    p.drawText(8, 16, "EMF Timeline (mG)");

    int plotX = 50, plotY = 24, plotW = w - 60, plotH = h - 34;

    /// We draw grid lines
    p.setPen(QPen(QColor(30, 30, 50), 1, Qt::DotLine));
    for (int i = 0; i <= 4; ++i) {
        int y = plotY + plotH * i / 4;
        p.drawLine(plotX, y, plotX + plotW, y);
    }

    /// We draw Y-axis scale
    p.setPen(QColor(100, 120, 110));
    p.setFont(QFont("Monospace", 8));
    for (int i = 0; i <= 4; ++i) {
        int y = plotY + plotH * i / 4;
        double val = max_emf_ * (4 - i) / 4.0;
        p.drawText(2, y + 4, QString::number(val, 'f', 1));
    }

    if (data_.size() < 2) {
        p.setPen(QColor(80, 80, 100));
        p.setFont(QFont("Monospace", 10));
        p.drawText(QRect(plotX, plotY, plotW, plotH), Qt::AlignCenter, "Waiting for EMF data...");
        return;
    }

    /// We draw the EMF trace line
    double timeStart = data_.front().timestamp;
    double timeEnd = data_.back().timestamp;
    double timeRange = std::max(timeEnd - timeStart, 1.0);

    QPen tracePen(QColor(0, 200, 150), 1.5);
    p.setPen(tracePen);

    for (size_t i = 1; i < data_.size(); ++i) {
        double x1 = plotX + plotW * (data_[i-1].timestamp - timeStart) / timeRange;
        double y1 = plotY + plotH * (1.0 - data_[i-1].emfMilligauss / max_emf_);
        double x2 = plotX + plotW * (data_[i].timestamp - timeStart) / timeRange;
        double y2 = plotY + plotH * (1.0 - data_[i].emfMilligauss / max_emf_);
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    /// We draw spike markers
    for (const auto& pt : data_) {
        if (!pt.isSpike) continue;
        double x = plotX + plotW * (pt.timestamp - timeStart) / timeRange;
        double y = plotY + plotH * (1.0 - pt.emfMilligauss / max_emf_);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 60, 60, 180));
        p.drawEllipse(QPointF(x, y), 4, 4);
        p.setPen(QColor(255, 100, 100));
        p.setFont(QFont("Monospace", 7));
        p.drawText(static_cast<int>(x) + 6, static_cast<int>(y) - 4,
                   QString::number(pt.emfMilligauss, 'f', 1) + " mG");
    }
}

} // namespace bthl::spiritbox
