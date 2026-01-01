/**
 * @file src/ui/EMFTimelineWidget.h
 * @title EMFTimelineWidget - EMF Data Timeline Chart
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We display a scrolling EMF timeline chart with spike markers,
 *          baseline tracking, and configurable time window.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial stub
 * 2026-04-12 - Full QPainter timeline implementation
 */

#ifndef BTHL_SPIRITBOX_EMF_TIMELINE_WIDGET_H
#define BTHL_SPIRITBOX_EMF_TIMELINE_WIDGET_H

#include <QWidget>
#include <deque>
#include <mutex>

namespace bthl::spiritbox {

struct EMFDataPoint {
    double timestamp{0.0};
    double emfMilligauss{0.0};
    bool isSpike{false};
};

class EMFTimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit EMFTimelineWidget(QWidget* parent = nullptr);

public slots:
    void addReading(double timestamp, double emfMilligauss, bool isSpike);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::deque<EMFDataPoint> data_;
    static constexpr size_t MAX_POINTS = 600;
    double time_window_sec_{60.0};
    double max_emf_{10.0};
    std::mutex mutex_;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_EMF_TIMELINE_WIDGET_H
