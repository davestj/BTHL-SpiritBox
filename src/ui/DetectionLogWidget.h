/**
 * @file src/ui/DetectionLogWidget.h
 * @title DetectionLogWidget - Filterable Detection Event Log
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We display a scrolling log of voice detection, transcription,
 *          EMF correlation, and anomaly events with filtering and markers.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial stub
 * 2026-04-12 - Full implementation with filtering and color-coded events
 */

#ifndef BTHL_SPIRITBOX_DETECTION_LOG_WIDGET_H
#define BTHL_SPIRITBOX_DETECTION_LOG_WIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QString>

namespace bthl::spiritbox {

class DetectionLogWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetectionLogWidget(QWidget* parent = nullptr);

public slots:
    void addVoiceEvent(double timestamp, double freqHz, float confidence);
    void addTranscription(double timestamp, const QString& text, float confidence);
    void addCorrelation(double timestamp, const QString& description);
    void addEMFSpike(double timestamp, double emfMilligauss);
    void clear();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void appendEntry(const QString& html);
    void rebuildFilter();

    QTextEdit* log_display_{nullptr};
    QCheckBox* show_voice_{nullptr};
    QCheckBox* show_transcription_{nullptr};
    QCheckBox* show_correlation_{nullptr};
    QCheckBox* show_emf_{nullptr};

    struct LogEntry {
        QString html;
        QString category;
        double timestamp{0.0};
    };
    std::vector<LogEntry> all_entries_;
    static constexpr size_t MAX_ENTRIES = 1000;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_DETECTION_LOG_WIDGET_H
