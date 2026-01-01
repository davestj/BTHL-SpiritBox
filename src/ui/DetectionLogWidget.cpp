/**
 * @file src/ui/DetectionLogWidget.cpp
 * @author David St John (davestj)
 * @date 2026-04-12
 * CHANGELOG:
 * 2026-03-30 - Stub
 * 2026-04-12 - Full implementation with color-coded events and filtering
 */
#include "ui/DetectionLogWidget.h"
#include <QHBoxLayout>
#include <QScrollBar>
#include <QFont>

namespace bthl::spiritbox {

DetectionLogWidget::DetectionLogWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    /// We create filter checkboxes
    auto* filterBar = new QHBoxLayout();
    show_voice_ = new QCheckBox("Voice");
    show_voice_->setChecked(true);
    show_voice_->setStyleSheet("QCheckBox { color: #00c896; font-size: 10px; }");
    connect(show_voice_, &QCheckBox::toggled, this, &DetectionLogWidget::rebuildFilter);

    show_transcription_ = new QCheckBox("Transcription");
    show_transcription_->setChecked(true);
    show_transcription_->setStyleSheet("QCheckBox { color: #64b5f6; font-size: 10px; }");
    connect(show_transcription_, &QCheckBox::toggled, this, &DetectionLogWidget::rebuildFilter);

    show_correlation_ = new QCheckBox("Correlation");
    show_correlation_->setChecked(true);
    show_correlation_->setStyleSheet("QCheckBox { color: #ffd740; font-size: 10px; }");
    connect(show_correlation_, &QCheckBox::toggled, this, &DetectionLogWidget::rebuildFilter);

    show_emf_ = new QCheckBox("EMF Spike");
    show_emf_->setChecked(true);
    show_emf_->setStyleSheet("QCheckBox { color: #ff5252; font-size: 10px; }");
    connect(show_emf_, &QCheckBox::toggled, this, &DetectionLogWidget::rebuildFilter);

    filterBar->addWidget(show_voice_);
    filterBar->addWidget(show_transcription_);
    filterBar->addWidget(show_correlation_);
    filterBar->addWidget(show_emf_);
    filterBar->addStretch();
    layout->addLayout(filterBar);

    /// We create the scrollable log display
    log_display_ = new QTextEdit();
    log_display_->setReadOnly(true);
    log_display_->setFont(QFont("Monospace", 9));
    log_display_->setStyleSheet(
        "QTextEdit { background-color: #0a0a1a; color: #c0c0d0; "
        "border: 1px solid #2a2a4a; }");
    layout->addWidget(log_display_);
}

void DetectionLogWidget::addVoiceEvent(double timestamp, double freqHz, float confidence) {
    LogEntry entry;
    entry.timestamp = timestamp;
    entry.category = "voice";
    entry.html = QString(
        "<span style='color:#00c896;'>[%1s] VOICE</span> "
        "<span style='color:#a0b0a0;'>%2 MHz | confidence %3%</span>")
        .arg(timestamp, 0, 'f', 1)
        .arg(freqHz / 1e6, 0, 'f', 3)
        .arg(static_cast<int>(confidence * 100));

    all_entries_.push_back(entry);
    while (all_entries_.size() > MAX_ENTRIES) all_entries_.erase(all_entries_.begin());
    appendEntry(entry.html);
}

void DetectionLogWidget::addTranscription(double timestamp, const QString& text, float confidence) {
    LogEntry entry;
    entry.timestamp = timestamp;
    entry.category = "transcription";
    entry.html = QString(
        "<span style='color:#64b5f6;'>[%1s] WHISPER</span> "
        "<span style='color:#e0e0f0; font-style:italic;'>\"%2\"</span> "
        "<span style='color:#808090;'>(%3%)</span>")
        .arg(timestamp, 0, 'f', 1)
        .arg(text.toHtmlEscaped())
        .arg(static_cast<int>(confidence * 100));

    all_entries_.push_back(entry);
    while (all_entries_.size() > MAX_ENTRIES) all_entries_.erase(all_entries_.begin());
    appendEntry(entry.html);
}

void DetectionLogWidget::addCorrelation(double timestamp, const QString& description) {
    LogEntry entry;
    entry.timestamp = timestamp;
    entry.category = "correlation";
    entry.html = QString(
        "<span style='color:#ffd740; font-weight:bold;'>[%1s] CORRELATED</span> "
        "<span style='color:#d0c090;'>%2</span>")
        .arg(timestamp, 0, 'f', 1)
        .arg(description.toHtmlEscaped());

    all_entries_.push_back(entry);
    while (all_entries_.size() > MAX_ENTRIES) all_entries_.erase(all_entries_.begin());
    appendEntry(entry.html);
}

void DetectionLogWidget::addEMFSpike(double timestamp, double emfMilligauss) {
    LogEntry entry;
    entry.timestamp = timestamp;
    entry.category = "emf";
    entry.html = QString(
        "<span style='color:#ff5252;'>[%1s] EMF SPIKE</span> "
        "<span style='color:#c08080;'>%2 mG</span>")
        .arg(timestamp, 0, 'f', 1)
        .arg(emfMilligauss, 0, 'f', 2);

    all_entries_.push_back(entry);
    while (all_entries_.size() > MAX_ENTRIES) all_entries_.erase(all_entries_.begin());
    appendEntry(entry.html);
}

void DetectionLogWidget::clear() {
    all_entries_.clear();
    log_display_->clear();
}

void DetectionLogWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void DetectionLogWidget::appendEntry(const QString& html) {
    log_display_->append(html);
    /// We auto-scroll to bottom
    auto* scrollBar = log_display_->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void DetectionLogWidget::rebuildFilter() {
    log_display_->clear();
    for (const auto& entry : all_entries_) {
        bool show = false;
        if (entry.category == "voice" && show_voice_->isChecked()) show = true;
        if (entry.category == "transcription" && show_transcription_->isChecked()) show = true;
        if (entry.category == "correlation" && show_correlation_->isChecked()) show = true;
        if (entry.category == "emf" && show_emf_->isChecked()) show = true;
        if (show) log_display_->append(entry.html);
    }
}

} // namespace bthl::spiritbox
