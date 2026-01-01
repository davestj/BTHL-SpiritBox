/**
 * @file src/ui/SweepVisualizerWidget.h
 * @title SweepVisualizerWidget - Real-time Spectrum Waterfall Display
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We display a scrolling waterfall spectrogram showing signal power
 *          across the sweep frequency range over time.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial stub
 * 2026-04-12 - Full QPainter waterfall implementation
 */

#ifndef BTHL_SPIRITBOX_SWEEP_VISUALIZER_WIDGET_H
#define BTHL_SPIRITBOX_SWEEP_VISUALIZER_WIDGET_H

#include <QWidget>
#include <deque>
#include <mutex>
#include <vector>

namespace bthl::spiritbox {

class SweepVisualizerWidget : public QWidget {
    Q_OBJECT
public:
    explicit SweepVisualizerWidget(QWidget* parent = nullptr);

public slots:
    void addSweepData(double freqHz, double powerDb, uint32_t step, uint32_t totalSteps);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor powerToColor(double powerDb) const;

    std::deque<std::vector<double>> waterfall_rows_;
    std::vector<double> current_row_;
    uint32_t expected_steps_{0};
    static constexpr size_t MAX_ROWS = 200;
    std::mutex mutex_;
    double min_power_db_{-80.0};
    double max_power_db_{-10.0};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SWEEP_VISUALIZER_WIDGET_H
