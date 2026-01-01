/**
 * @file src/ui/MainWindow.h
 * @title MainWindow - BTHL-SpiritBox Primary Application Window
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We provide the main application window that houses all subsystem controls:
 *          sweep engine, audio output, voice detection, EMF monitoring, session recording,
 *          and visualization widgets. This is the central orchestrator of the application.
 * @reason A single window with dockable panels gives investigators full control of all
 *         sensors and detection systems during a live investigation session.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with dock-based layout
 */

#ifndef BTHL_SPIRITBOX_MAIN_WINDOW_H
#define BTHL_SPIRITBOX_MAIN_WINDOW_H

#include "core/SweepEngine.h"
#include "core/AudioDemodulator.h"
#include "audio/AudioOutputManager.h"
#include "detection/VoiceActivityDetector.h"
#include "detection/WhisperTranscriber.h"
#include "emf/EMFSerialReader.h"
#include "emf/EMFCorrelator.h"
#include "session/SessionRecorder.h"
#include "session/SessionPlayer.h"
#include "ui/SweepVisualizerWidget.h"
#include "ui/AudioWaveformWidget.h"
#include "ui/EMFTimelineWidget.h"
#include "ui/DetectionLogWidget.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>
#include <memory>

namespace bthl::spiritbox {

/**
 * @class MainWindow
 * @purpose We build the main application window and wire all subsystems together
 *          through Qt signal/slot connections.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // ─── Device Management ─────────────────────────────────────────────────
    void onRefreshSDRDevices();
    void onOpenSDRDevice();
    void onCloseSDRDevice();
    void onRefreshEMFPorts();
    void onConnectEMF();
    void onDisconnectEMF();

    // ─── Sweep Control ─────────────────────────────────────────────────────
    void onStartSweep();
    void onStopSweep();
    void onProfileChanged(int index);
    void onDwellTimeChanged(int value);
    void onGainChanged(int value);

    // ─── Session Control ───────────────────────────────────────────────────
    void onStartRecording();
    void onStopRecording();
    void onLoadSession();

    // ─── Whisper Control ───────────────────────────────────────────────────
    void onLoadWhisperModel();

    // ─── Status Updates ────────────────────────────────────────────────────
    void onSweepStatusUpdated(const SweepStatus& status);
    void onVoiceDetected(const VoiceDetectionEvent& event);
    void onTranscriptionReady(const TranscriptionResult& result);
    void onCorrelatedEvent(const CorrelatedEvent& event);
    void onVADMetricsUpdated(float energy, float zcr, float flatness, float confidence);
    void onEMFReading(const EMFReading& reading);
    void onUpdateStatusBar();

private:
    // ─── UI Construction ───────────────────────────────────────────────────
    void createMenuBar();
    void createToolBar();
    void createCentralWidget();
    void createControlDock();
    void createDetectionDock();
    void createEMFDock();
    void createTranscriptionDock();
    void createStatusBar();

    // ─── Subsystem Wiring ──────────────────────────────────────────────────
    void wireSignals();
    void loadDefaultProfile();

    // ─── Subsystem Instances ───────────────────────────────────────────────
    std::unique_ptr<SweepEngine> m_sweepEngine;
    std::unique_ptr<AudioDemodulator> m_demodulator;
    std::unique_ptr<AudioOutputManager> m_audioOutput;
    std::unique_ptr<VoiceActivityDetector> m_vad;
    std::unique_ptr<WhisperTranscriber> m_whisper;
    std::unique_ptr<EMFSerialReader> m_emfReader;
    std::unique_ptr<EMFCorrelator> m_correlator;
    std::unique_ptr<SessionRecorder> m_recorder;
    std::unique_ptr<SessionPlayer> m_player;

    // ─── Control Widgets ───────────────────────────────────────────────────
    QComboBox* m_sdrDeviceCombo{nullptr};
    QComboBox* m_profileCombo{nullptr};
    QComboBox* m_emfPortCombo{nullptr};
    QPushButton* m_sdrOpenBtn{nullptr};
    QPushButton* m_sweepStartBtn{nullptr};
    QPushButton* m_sweepStopBtn{nullptr};
    QPushButton* m_emfConnectBtn{nullptr};
    QPushButton* m_recordBtn{nullptr};
    QPushButton* m_stopRecordBtn{nullptr};
    QPushButton* m_loadModelBtn{nullptr};
    QSlider* m_dwellSlider{nullptr};
    QSlider* m_gainSlider{nullptr};
    QSlider* m_volumeSlider{nullptr};
    QLabel* m_dwellLabel{nullptr};
    QLabel* m_gainLabel{nullptr};
    QLabel* m_freqLabel{nullptr};
    QLabel* m_emfLabel{nullptr};

    // ─── Display Widgets ───────────────────────────────────────────────────
    QTextEdit* m_detectionLog{nullptr};
    QTextEdit* m_transcriptionLog{nullptr};

    // ─── Visualization Widgets (Phase 2) ──────────────────────────────────
    SweepVisualizerWidget* m_sweepVisualizer{nullptr};
    AudioWaveformWidget* m_audioWaveform{nullptr};
    EMFTimelineWidget* m_emfTimeline{nullptr};
    DetectionLogWidget* m_detectionLogWidget{nullptr};

    // ─── Status Bar ────────────────────────────────────────────────────────
    QLabel* m_statusFreq{nullptr};
    QLabel* m_statusEMF{nullptr};
    QLabel* m_statusVAD{nullptr};
    QLabel* m_statusRecording{nullptr};
    QTimer* m_statusTimer{nullptr};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_MAIN_WINDOW_H
