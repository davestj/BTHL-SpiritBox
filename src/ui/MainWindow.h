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
#include "audio/MicrophoneInput.h"
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
#include "ui/HelpBrowser.h"
#ifdef __APPLE__
#include "sensors/WirelessScanner.h"
#endif

#include <QMainWindow>
#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QSlider>
#include <QLabel>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>
#include <QHash>
#include <memory>

namespace bthl::spiritbox {

/**
 * @enum CaptureMode
 * @purpose We define how a session is captured. The mode governs whether the investigator
 *          microphone is recorded and how the transcript/session is framed.
 */
enum class CaptureMode {
    Interactive,    ///< Mic ON — investigator questions + radio responses (full Q&A)
    Standalone,     ///< Mic OFF — radio responses only
    PassiveListen   ///< Mic OFF — continuous listening for spontaneous responses
};

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
    void onUnloadWhisperModel();
    void onDownloadModels();

    // ─── Updates ───────────────────────────────────────────────────────────
    void onCheckForUpdates(bool silent);

    // ─── Capture Modes + Transcript Export + Help ──────────────────────────
    void onCaptureModeChanged(CaptureMode mode);
    void onSaveTranscript();
    void onShowDocumentation();
    void onShowDeviceCapabilities();

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

    // ─── Dock behavior ─────────────────────────────────────────────────────
    /// We make every dock panel re-dock instead of vanishing when its close button is used.
    void installDockRedockBehavior();

protected:
    /// We intercept dock-widget close events to snap the panel back to its dock area.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:

    // ─── Subsystem Instances ───────────────────────────────────────────────
    std::unique_ptr<SweepEngine> m_sweepEngine;
    std::unique_ptr<AudioDemodulator> m_demodulator;
    std::unique_ptr<AudioOutputManager> m_audioOutput;
    std::unique_ptr<VoiceActivityDetector> m_vad;
    std::unique_ptr<MicrophoneInput> m_micInput;             ///< Investigator microphone capture
    std::unique_ptr<VoiceActivityDetector> m_investigatorVad; ///< VAD for the investigator mic path
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
    QPushButton* m_unloadModelBtn{nullptr};
    QPushButton* m_saveTranscriptBtn{nullptr};

    // ─── Capture Mode selector ─────────────────────────────────────────────
    QButtonGroup* m_modeGroup{nullptr};
    QPushButton* m_modeInteractiveBtn{nullptr};
    QPushButton* m_modeStandaloneBtn{nullptr};
    QPushButton* m_modePassiveBtn{nullptr};
    CaptureMode m_captureMode{CaptureMode::Standalone};
    QSlider* m_dwellSlider{nullptr};
    QSlider* m_gainSlider{nullptr};
    QSlider* m_volumeSlider{nullptr};
    QLabel* m_dwellLabel{nullptr};
    QLabel* m_gainLabel{nullptr};
    QLabel* m_freqLabel{nullptr};
    QLabel* m_emfLabel{nullptr};

    // ─── Display Widgets ───────────────────────────────────────────────────
    // We use QPlainTextEdit with a bounded block count for these high-volume logs: it is built
    // for fast appends and auto-trims old lines, avoiding the unbounded-growth layout crash a
    // QTextEdit hits under continuous transcription.
    QPlainTextEdit* m_detectionLog{nullptr};
    QPlainTextEdit* m_transcriptionLog{nullptr};

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
    QLabel* m_statusMode{nullptr};
    QLabel* m_statusWireless{nullptr};
    QTimer* m_statusTimer{nullptr};

    // We remember each dock's home area so we can snap it back when closed.
    QHash<QDockWidget*, Qt::DockWidgetArea> m_dockHomeAreas;

    // ─── Documentation browser (lazily created) ────────────────────────────
    std::unique_ptr<HelpBrowser> m_helpBrowser;
#ifdef __APPLE__
    std::unique_ptr<WirelessScanner> m_wireless;   ///< macOS Wi-Fi + BLE environmental scanner
#endif
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_MAIN_WINDOW_H
