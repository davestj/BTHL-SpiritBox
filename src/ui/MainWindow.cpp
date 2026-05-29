/**
 * @file src/ui/MainWindow.cpp
 * @title MainWindow Implementation
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We implement the main application window with dock-based layout, all subsystem
 *          initialization, signal/slot wiring, and UI event handling.
 * @reason This is the orchestrator that ties SDR sweep, audio, VAD, Whisper, EMF, and
 *         session recording into a cohesive investigation tool.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with full subsystem wiring
 */

#include "ui/MainWindow.h"
#include "hub/SensorHub.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QDir>
#include <QDateTime>
#include <QUuid>

namespace bthl::spiritbox {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_sweepEngine(std::make_unique<SweepEngine>())
    , m_demodulator(std::make_unique<AudioDemodulator>())
    , m_audioOutput(std::make_unique<AudioOutputManager>())
    , m_vad(std::make_unique<VoiceActivityDetector>())
    , m_whisper(std::make_unique<WhisperTranscriber>())
    , m_emfReader(std::make_unique<EMFSerialReader>())
    , m_correlator(std::make_unique<EMFCorrelator>())
    , m_recorder(std::make_unique<SessionRecorder>())
    , m_player(std::make_unique<SessionPlayer>()) {

    setWindowTitle("BTHL-SpiritBox v1.0.0 - Beyond The Horizon Labs");
    setMinimumSize(1200, 800);
    resize(1400, 900);

    createMenuBar();
    createToolBar();
    createCentralWidget();
    createControlDock();
    createDetectionDock();
    createEMFDock();
    createTranscriptionDock();
    createStatusBar();
    wireSignals();
    loadDefaultProfile();

    qInfo() << "MainWindow: We initialized BTHL-SpiritBox application";

    /// We auto-detect SDR devices on startup and open the first one found
    QTimer::singleShot(500, this, [this]() {
        onRefreshSDRDevices();
        if (m_sdrDeviceCombo->count() > 0 &&
            m_sdrDeviceCombo->itemText(0) != "No SDR devices found") {
            m_sdrDeviceCombo->setCurrentIndex(0);
            onOpenSDRDevice();
            statusBar()->showMessage("SDR device auto-connected — ready to sweep", 3000);
        }

        /// We also auto-detect and connect the EMF-390 if present
        onRefreshEMFPorts();
        if (m_emfPortCombo->count() > 0) {
            m_emfPortCombo->setCurrentIndex(0);
            onConnectEMF();
        }

        /// We stop any running sweep (may have been started on AM profile)
        if (m_sweepEngine->isSweeping()) {
            m_sweepEngine->stopSweep();
        }

        /// We force FM profile (AM is below E4000 52 MHz minimum)
        m_profileCombo->blockSignals(true);
        m_profileCombo->setCurrentIndex(1);
        m_profileCombo->blockSignals(false);
        auto fmProfile = SweepProfile::createFMBroadcast();
        m_sweepEngine->setProfile(fmProfile);
        m_demodulator->setAudioSampleRate(fmProfile.audioSampleRate());
        m_demodulator->setSdrSampleRate(fmProfile.sdrSampleRate());
        qInfo() << "MainWindow: Forced FM Broadcast profile (88-108 MHz)";

        /// We auto-start the sweep on FM
        if (m_sweepEngine->isDeviceOpen()) {
            statusBar()->showMessage("Sweeping FM 88-108 MHz...", 3000);
            onStartSweep();
        }
    });
}

MainWindow::~MainWindow() {
    m_sweepEngine->stopSweep();
    m_audioOutput->shutdown();
    m_emfReader->disconnectDevice();
}

// ─── Menu Bar ──────────────────────────────────────────────────────────────────

void MainWindow::createMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Load Session...", this, &MainWindow::onLoadSession);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", qApp, &QApplication::quit);

    auto* deviceMenu = menuBar()->addMenu("&Devices");
    deviceMenu->addAction("Refresh &SDR Devices", this, &MainWindow::onRefreshSDRDevices);
    deviceMenu->addAction("Refresh &EMF Ports", this, &MainWindow::onRefreshEMFPorts);

    auto* whisperMenu = menuBar()->addMenu("&Whisper");
    whisperMenu->addAction("&Load Model...", this, &MainWindow::onLoadWhisperModel);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", [this]() {
        QMessageBox::about(this, "About BTHL-SpiritBox",
            "BTHL-SpiritBox v1.0.0\n\n"
            "Multi-sensor paranormal audio detection system\n"
            "SDR + EMF + AI Transcription\n\n"
            "Beyond The Horizon Labs\n"
            "Author: David St John (davestj)\n"
            "https://github.com/davestj");
    });
}

// ─── Tool Bar ──────────────────────────────────────────────────────────────────

void MainWindow::createToolBar() {
    auto* toolbar = addToolBar("Main Controls");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24));

    m_sweepStartBtn = new QPushButton("START SWEEP");
    m_sweepStartBtn->setStyleSheet("QPushButton { background-color: #2d8a4e; color: white; "
                                    "font-weight: bold; padding: 8px 16px; border-radius: 4px; }");
    connect(m_sweepStartBtn, &QPushButton::clicked, this, &MainWindow::onStartSweep);
    toolbar->addWidget(m_sweepStartBtn);

    m_sweepStopBtn = new QPushButton("STOP SWEEP");
    m_sweepStopBtn->setStyleSheet("QPushButton { background-color: #c0392b; color: white; "
                                   "font-weight: bold; padding: 8px 16px; border-radius: 4px; }");
    m_sweepStopBtn->setEnabled(false);
    connect(m_sweepStopBtn, &QPushButton::clicked, this, &MainWindow::onStopSweep);
    toolbar->addWidget(m_sweepStopBtn);

    toolbar->addSeparator();

    m_recordBtn = new QPushButton("REC");
    m_recordBtn->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; "
                                "font-weight: bold; padding: 8px 12px; border-radius: 4px; }");
    connect(m_recordBtn, &QPushButton::clicked, this, &MainWindow::onStartRecording);
    toolbar->addWidget(m_recordBtn);

    m_stopRecordBtn = new QPushButton("STOP REC");
    m_stopRecordBtn->setEnabled(false);
    connect(m_stopRecordBtn, &QPushButton::clicked, this, &MainWindow::onStopRecording);
    toolbar->addWidget(m_stopRecordBtn);

    toolbar->addSeparator();

    // We add the volume control to the toolbar
    toolbar->addWidget(new QLabel(" Volume: "));
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setMaximumWidth(120);
    connect(m_volumeSlider, &QSlider::valueChanged, [this](int val) {
        m_audioOutput->setVolume(static_cast<float>(val) / 100.0f);
    });
    toolbar->addWidget(m_volumeSlider);
}

// ─── Central Widget ────────────────────────────────────────────────────────────

void MainWindow::createCentralWidget() {
    auto* centralWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(centralWidget);

    // We display the current frequency prominently
    m_freqLabel = new QLabel("-- --- --- Hz");
    m_freqLabel->setAlignment(Qt::AlignCenter);
    QFont freqFont("Courier", 36, QFont::Bold);
    m_freqLabel->setFont(freqFont);
    m_freqLabel->setStyleSheet("QLabel { color: #00ff88; background-color: #1a1a2e; "
                                "padding: 20px; border-radius: 8px; }");
    layout->addWidget(m_freqLabel);

    /// We add the spectrum waterfall visualizer
    m_sweepVisualizer = new SweepVisualizerWidget();
    m_sweepVisualizer->setMinimumHeight(200);
    layout->addWidget(m_sweepVisualizer, 2);

    /// We add the audio waveform with VU meter and VAD confidence
    m_audioWaveform = new AudioWaveformWidget();
    m_audioWaveform->setMinimumHeight(120);
    layout->addWidget(m_audioWaveform, 1);

    setCentralWidget(centralWidget);
}

// ─── Control Dock ──────────────────────────────────────────────────────────────

void MainWindow::createControlDock() {
    auto* dock = new QDockWidget("Controls", this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    // We build the SDR device selector
    auto* sdrGroup = new QGroupBox("SDR Device");
    auto* sdrLayout = new QVBoxLayout(sdrGroup);
    m_sdrDeviceCombo = new QComboBox();
    sdrLayout->addWidget(m_sdrDeviceCombo);

    auto* sdrBtnLayout = new QHBoxLayout();
    auto* refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshSDRDevices);
    sdrBtnLayout->addWidget(refreshBtn);

    m_sdrOpenBtn = new QPushButton("Open");
    connect(m_sdrOpenBtn, &QPushButton::clicked, this, &MainWindow::onOpenSDRDevice);
    sdrBtnLayout->addWidget(m_sdrOpenBtn);
    sdrLayout->addLayout(sdrBtnLayout);
    layout->addWidget(sdrGroup);

    // We build the sweep profile selector
    auto* profileGroup = new QGroupBox("Sweep Profile");
    auto* profileLayout = new QVBoxLayout(profileGroup);
    m_profileCombo = new QComboBox();
    m_profileCombo->blockSignals(true);
    m_profileCombo->addItem("AM Broadcast (530-1700 kHz)");
    m_profileCombo->addItem("FM Broadcast (88-108 MHz)");
    m_profileCombo->addItem("VHF Low Band (30-88 MHz)");
    m_profileCombo->addItem("Full Spectrum (AM+VHF+FM)");
    m_profileCombo->setCurrentIndex(1);  // FM default
    m_profileCombo->blockSignals(false);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProfileChanged);
    profileLayout->addWidget(m_profileCombo);
    layout->addWidget(profileGroup);

    // We build the dwell time slider
    auto* dwellGroup = new QGroupBox("Dwell Time");
    auto* dwellLayout = new QVBoxLayout(dwellGroup);
    m_dwellSlider = new QSlider(Qt::Horizontal);
    m_dwellSlider->setRange(10, 200);
    m_dwellSlider->setValue(50);
    connect(m_dwellSlider, &QSlider::valueChanged, this, &MainWindow::onDwellTimeChanged);
    dwellLayout->addWidget(m_dwellSlider);
    m_dwellLabel = new QLabel("50 ms");
    m_dwellLabel->setAlignment(Qt::AlignCenter);
    dwellLayout->addWidget(m_dwellLabel);
    layout->addWidget(dwellGroup);

    // We build the gain slider
    auto* gainGroup = new QGroupBox("SDR Gain");
    auto* gainLayout = new QVBoxLayout(gainGroup);
    m_gainSlider = new QSlider(Qt::Horizontal);
    m_gainSlider->setRange(0, 50);
    m_gainSlider->setValue(0);
    connect(m_gainSlider, &QSlider::valueChanged, this, &MainWindow::onGainChanged);
    gainLayout->addWidget(m_gainSlider);
    m_gainLabel = new QLabel("AGC");
    m_gainLabel->setAlignment(Qt::AlignCenter);
    gainLayout->addWidget(m_gainLabel);
    layout->addWidget(gainGroup);

    layout->addStretch();
    dock->setWidget(widget);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

// ─── Detection Log Dock ───────────────────────────────────────────────────────

void MainWindow::createDetectionDock() {
    auto* dock = new QDockWidget("Detection Log", this);
    dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    /// We use the filterable DetectionLogWidget instead of plain QTextEdit
    m_detectionLogWidget = new DetectionLogWidget();
    dock->setWidget(m_detectionLogWidget);

    /// We also keep the simple text log for backward compatibility
    m_detectionLog = new QTextEdit();
    m_detectionLog->setReadOnly(true);
    m_detectionLog->setVisible(false);

    addDockWidget(Qt::BottomDockWidgetArea, dock);
}

// ─── EMF Dock ──────────────────────────────────────────────────────────────────

void MainWindow::createEMFDock() {
    auto* dock = new QDockWidget("EMF Monitor", this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    // We build the EMF port selector
    m_emfPortCombo = new QComboBox();
    layout->addWidget(m_emfPortCombo);

    auto* btnLayout = new QHBoxLayout();
    auto* refreshEmfBtn = new QPushButton("Refresh");
    connect(refreshEmfBtn, &QPushButton::clicked, this, &MainWindow::onRefreshEMFPorts);
    btnLayout->addWidget(refreshEmfBtn);

    m_emfConnectBtn = new QPushButton("Connect");
    connect(m_emfConnectBtn, &QPushButton::clicked, this, &MainWindow::onConnectEMF);
    btnLayout->addWidget(m_emfConnectBtn);
    layout->addLayout(btnLayout);

    // We display the current EMF reading
    m_emfLabel = new QLabel("EMF: -- mG");
    m_emfLabel->setAlignment(Qt::AlignCenter);
    m_emfLabel->setFont(QFont("Courier", 24, QFont::Bold));
    m_emfLabel->setStyleSheet("QLabel { color: #ff6b6b; background-color: #1a1a2e; "
                               "padding: 15px; border-radius: 8px; }");
    layout->addWidget(m_emfLabel);

    /// We add the EMF timeline chart below the current reading
    m_emfTimeline = new EMFTimelineWidget();
    m_emfTimeline->setMinimumHeight(150);
    layout->addWidget(m_emfTimeline, 1);
    dock->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

// ─── Transcription Dock ────────────────────────────────────────────────────────

void MainWindow::createTranscriptionDock() {
    auto* dock = new QDockWidget("Transcriptions", this);
    dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    m_loadModelBtn = new QPushButton("Load Whisper Model...");
    connect(m_loadModelBtn, &QPushButton::clicked, this, &MainWindow::onLoadWhisperModel);
    layout->addWidget(m_loadModelBtn);

    m_transcriptionLog = new QTextEdit();
    m_transcriptionLog->setReadOnly(true);
    m_transcriptionLog->setFont(QFont("Courier", 11));
    m_transcriptionLog->setStyleSheet("QTextEdit { background-color: #0d1117; color: #ffd700; }");
    layout->addWidget(m_transcriptionLog);

    dock->setWidget(widget);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
}

// ─── Status Bar ────────────────────────────────────────────────────────────────

void MainWindow::createStatusBar() {
    m_statusFreq = new QLabel("Freq: Idle");
    m_statusEMF = new QLabel("EMF: N/A");
    m_statusVAD = new QLabel("VAD: Idle");
    m_statusRecording = new QLabel("REC: Off");

    statusBar()->addPermanentWidget(m_statusFreq);
    statusBar()->addPermanentWidget(m_statusEMF);
    statusBar()->addPermanentWidget(m_statusVAD);
    statusBar()->addPermanentWidget(m_statusRecording);

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onUpdateStatusBar);
    m_statusTimer->start(250);
}

// ─── Signal Wiring ─────────────────────────────────────────────────────────────

void MainWindow::wireSignals() {
    // We connect the sweep engine IQ output to the demodulator
    connect(m_sweepEngine.get(), &SweepEngine::iqSamplesReady,
            m_demodulator.get(), &AudioDemodulator::processIQSamples);

    // We connect the demodulator audio output to the audio output manager's ring buffer
    connect(m_demodulator.get(), &AudioDemodulator::audioReady,
            [this](const std::vector<float>& samples, double /*freqHz*/, uint32_t /*rate*/) {
                m_audioOutput->ringBuffer().write(samples);
            });

    // We connect the demodulator audio to the voice activity detector
    connect(m_demodulator.get(), &AudioDemodulator::audioReady,
            m_vad.get(), &VoiceActivityDetector::processAudio);

    // We connect VAD detections to the Whisper transcriber
    connect(m_vad.get(), &VoiceActivityDetector::voiceDetected,
            m_whisper.get(), &WhisperTranscriber::onVoiceDetected);

    // We connect VAD detections to the EMF correlator
    connect(m_vad.get(), &VoiceActivityDetector::voiceDetected,
            m_correlator.get(), &EMFCorrelator::onVoiceDetected);

    // We connect EMF spikes to the correlator
    connect(m_emfReader.get(), &EMFSerialReader::spikeDetected,
            m_correlator.get(), &EMFCorrelator::onEMFSpike);

    // We connect continuous EMF readings to the correlator for baseline tracking
    connect(m_emfReader.get(), &EMFSerialReader::readingReceived,
            m_correlator.get(), &EMFCorrelator::onEMFReading);

    // We connect Whisper transcriptions to the correlator
    connect(m_whisper.get(), &WhisperTranscriber::transcriptionReady,
            m_correlator.get(), &EMFCorrelator::onTranscriptionReady);

    // ─── Session Recording Connections ─────────────────────────────────────

    // We connect audio to session recorder
    connect(m_demodulator.get(), &AudioDemodulator::audioReady,
            m_recorder.get(), &SessionRecorder::onAudioSamples);

    // We connect EMF readings to session recorder
    connect(m_emfReader.get(), &EMFSerialReader::readingReceived,
            m_recorder.get(), &SessionRecorder::onEMFReading);

    // We connect voice detections to session recorder
    connect(m_vad.get(), &VoiceActivityDetector::voiceDetected,
            m_recorder.get(), &SessionRecorder::onVoiceDetected);

    // We connect transcriptions to session recorder
    connect(m_whisper.get(), &WhisperTranscriber::transcriptionReady,
            m_recorder.get(), &SessionRecorder::onTranscriptionReady);

    // We connect correlated events to session recorder
    connect(m_correlator.get(), &EMFCorrelator::correlatedEventDetected,
            m_recorder.get(), &SessionRecorder::onCorrelatedEvent);

    // ─── UI Update Connections ─────────────────────────────────────────────

    connect(m_sweepEngine.get(), &SweepEngine::sweepStatusUpdated,
            this, &MainWindow::onSweepStatusUpdated);

    connect(m_vad.get(), &VoiceActivityDetector::voiceDetected,
            this, &MainWindow::onVoiceDetected);

    connect(m_vad.get(), &VoiceActivityDetector::metricsUpdated,
            this, &MainWindow::onVADMetricsUpdated);

    connect(m_whisper.get(), &WhisperTranscriber::transcriptionReady,
            this, &MainWindow::onTranscriptionReady);

    connect(m_correlator.get(), &EMFCorrelator::correlatedEventDetected,
            this, &MainWindow::onCorrelatedEvent);

    connect(m_emfReader.get(), &EMFSerialReader::readingReceived,
            this, &MainWindow::onEMFReading);

    /// We connect error signals to the status bar so the user sees failures
    connect(m_sweepEngine.get(), &SweepEngine::errorOccurred,
            [this](const QString& error) {
                statusBar()->showMessage("Error: " + error, 5000);
                qWarning() << "SweepEngine error:" << error;
            });

    // ─── Visualization Widget Connections (Phase 2) ───────────────────────

    /// We feed spectrum power data to the waterfall visualizer
    /// We use sweepStatusUpdated which gives us freq, power, step, and total
    connect(m_sweepEngine.get(), &SweepEngine::sweepStatusUpdated,
            [this](const SweepStatus& status) {
                m_sweepVisualizer->addSweepData(status.currentFreqHz,
                    status.signalPowerDb, status.currentStep, status.totalSteps);
            });

    /// We feed demodulated audio to the waveform display
    connect(m_demodulator.get(), &AudioDemodulator::audioReady,
            [this](const std::vector<float>& samples, double /*freqHz*/, uint32_t /*rate*/) {
                m_audioWaveform->addAudioSamples(samples);
            });

    /// We feed VAD metrics to the waveform confidence bar
    connect(m_vad.get(), &VoiceActivityDetector::metricsUpdated,
            m_audioWaveform, &AudioWaveformWidget::setVADMetrics);

    /// We feed EMF readings to the timeline widget (if created in dock)
    if (m_emfTimeline) {
        connect(m_emfReader.get(), &EMFSerialReader::readingReceived,
                [this](const EMFReading& reading) {
                    m_emfTimeline->addReading(reading.timestamp, reading.emfMilligauss,
                                              reading.isSpike);
                });
    }

    /// We feed detection events to the filterable detection log widget
    if (m_detectionLogWidget) {
        connect(m_vad.get(), &VoiceActivityDetector::voiceDetected,
                [this](const VoiceDetectionEvent& event) {
                    m_detectionLogWidget->addVoiceEvent(event.timestamp,
                        event.frequencyHz, event.confidence);
                });

        connect(m_whisper.get(), &WhisperTranscriber::transcriptionReady,
                [this](const TranscriptionResult& result) {
                    m_detectionLogWidget->addTranscription(result.timestamp,
                        result.text, result.confidence);
                });

        connect(m_correlator.get(), &EMFCorrelator::correlatedEventDetected,
                [this](const CorrelatedEvent& event) {
                    QString desc = QString("EMF %1 mG + Voice %2% @ %3 MHz (dt=%4ms, score=%5)")
                        .arg(event.emfReading.emfMilligauss, 0, 'f', 1)
                        .arg(static_cast<int>(event.voiceEvent.confidence * 100))
                        .arg(event.voiceEvent.frequencyHz / 1e6, 0, 'f', 3)
                        .arg(event.timeDeltaMs, 0, 'f', 0)
                        .arg(event.correlationScore, 0, 'f', 2);
                    m_detectionLogWidget->addCorrelation(event.timestamp, desc);
                });

        connect(m_emfReader.get(), &EMFSerialReader::spikeDetected,
                [this](const EMFReading& reading) {
                    m_detectionLogWidget->addEMFSpike(reading.timestamp,
                        reading.emfMilligauss);
                });
    }
}

void MainWindow::loadDefaultProfile() {
    /// We load FM Broadcast by default — AM is below E4000 tuner range (52 MHz min)
    auto fmProfile = SweepProfile::createFMBroadcast();
    m_sweepEngine->setProfile(fmProfile);
    m_demodulator->setAudioSampleRate(fmProfile.audioSampleRate());
    m_demodulator->setSdrSampleRate(fmProfile.sdrSampleRate());
    if (!m_audioOutput->initialize(fmProfile.audioSampleRate(), 512)) {
        qWarning() << "MainWindow: We could not initialize audio output";
    }
}

// ─── Slot Implementations ──────────────────────────────────────────────────────

void MainWindow::onRefreshSDRDevices() {
    m_sdrDeviceCombo->clear();
    auto devices = SweepEngine::enumerateDevices();
    for (const auto& dev : devices) {
        m_sdrDeviceCombo->addItem(
            QString("%1 [%2]").arg(dev.label, dev.driver),
            QVariant::fromValue(static_cast<int>(m_sdrDeviceCombo->count())));
    }
    if (devices.empty()) {
        m_sdrDeviceCombo->addItem("No SDR devices found");
    }
}

void MainWindow::onOpenSDRDevice() {
    auto devices = SweepEngine::enumerateDevices();
    int idx = m_sdrDeviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(devices.size())) return;

    if (m_sweepEngine->openDevice(devices[static_cast<size_t>(idx)])) {
        m_sdrOpenBtn->setText("Close");
        disconnect(m_sdrOpenBtn, &QPushButton::clicked, this, &MainWindow::onOpenSDRDevice);
        connect(m_sdrOpenBtn, &QPushButton::clicked, this, &MainWindow::onCloseSDRDevice);
        statusBar()->showMessage("We connected to SDR device", 3000);
    }
}

void MainWindow::onCloseSDRDevice() {
    m_sweepEngine->closeDevice();
    m_sdrOpenBtn->setText("Open");
    disconnect(m_sdrOpenBtn, &QPushButton::clicked, this, &MainWindow::onCloseSDRDevice);
    connect(m_sdrOpenBtn, &QPushButton::clicked, this, &MainWindow::onOpenSDRDevice);
}

void MainWindow::onRefreshEMFPorts() {
    m_emfPortCombo->clear();
    auto ports = EMFSerialReader::enumerateDevices();
    for (const auto& port : ports) {
        m_emfPortCombo->addItem(QString("%1 (%2)").arg(port.portName(), port.description()));
    }
}

void MainWindow::onConnectEMF() {
    auto ports = EMFSerialReader::enumerateDevices();
    int idx = m_emfPortCombo->currentIndex();
    if (idx < 0 || idx >= ports.size()) return;

    if (m_emfReader->connectDevice(ports[idx].portName())) {
        m_emfReader->startReading();
        m_emfConnectBtn->setText("Disconnect");
        disconnect(m_emfConnectBtn, &QPushButton::clicked, this, &MainWindow::onConnectEMF);
        connect(m_emfConnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectEMF);
    }
}

void MainWindow::onDisconnectEMF() {
    m_emfReader->disconnectDevice();
    m_emfConnectBtn->setText("Connect");
    disconnect(m_emfConnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectEMF);
    connect(m_emfConnectBtn, &QPushButton::clicked, this, &MainWindow::onConnectEMF);
}

void MainWindow::onStartSweep() {
    qInfo() << "MainWindow: Start Sweep clicked";
    m_audioOutput->start();
    qInfo() << "MainWindow: Audio output started";
    m_sweepEngine->startSweep();
    qInfo() << "MainWindow: Sweep engine started";
    m_sweepStartBtn->setEnabled(false);
    m_sweepStopBtn->setEnabled(true);
}

void MainWindow::onStopSweep() {
    m_sweepEngine->stopSweep();
    m_audioOutput->stop();
    m_sweepStartBtn->setEnabled(true);
    m_sweepStopBtn->setEnabled(false);
    m_freqLabel->setText("-- --- --- Hz");
}

void MainWindow::onProfileChanged(int index) {
    SweepProfile profile;
    switch (index) {
        case 0: profile = SweepProfile::createAMBroadcast(); break;
        case 1: profile = SweepProfile::createFMBroadcast(); break;
        case 2: profile = SweepProfile::createVHFLow(); break;
        case 3: profile = SweepProfile::createFullSpectrum(); break;
        default: return;
    }

    /// We warn and redirect if AM is selected but tuner can't reach AM frequencies
    if (index == 0) {
        auto devInfo = m_sweepEngine->currentDeviceInfo();
        if (!devInfo.label.isEmpty()) {
            /// E4000 tuner min is 52 MHz — AM band (530 kHz) is unreachable
            statusBar()->showMessage("AM band requires a tuner below 52 MHz — using FM instead", 5000);
            qWarning() << "MainWindow: AM profile incompatible with current tuner, switching to FM";
            m_profileCombo->blockSignals(true);
            m_profileCombo->setCurrentIndex(1);
            m_profileCombo->blockSignals(false);
            profile = SweepProfile::createFMBroadcast();
        }
    }

    m_sweepEngine->setProfile(profile);
    m_demodulator->setAudioSampleRate(profile.audioSampleRate());
    m_demodulator->setSdrSampleRate(profile.sdrSampleRate());
}

void MainWindow::onDwellTimeChanged(int value) {
    m_dwellLabel->setText(QString("%1 ms").arg(value));
    m_sweepEngine->setDwellTimeMs(static_cast<uint32_t>(value));
}

void MainWindow::onGainChanged(int value) {
    if (value == 0) {
        m_gainLabel->setText("AGC");
    } else {
        m_gainLabel->setText(QString("%1 dB").arg(value));
    }
    m_sweepEngine->setGain(static_cast<double>(value));
}

void MainWindow::onStartRecording() {
    /// We create a session directory and start recording
    QString sessionDir = QDir::homePath() + "/BTHL-SpiritBox/sessions/" +
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QDir().mkpath(sessionDir);

    InvestigationSession session;
    session.sessionId = QUuid::createUuid();
    session.name = "Live Investigation";
    session.startTime = QDateTime::currentDateTime();

    if (m_recorder->startRecording(sessionDir, session)) {
        m_recordBtn->setEnabled(false);
        m_stopRecordBtn->setEnabled(true);
        m_statusRecording->setText("REC: RECORDING");
        m_statusRecording->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    }
}

void MainWindow::onStopRecording() {
    m_recorder->stopRecording();
    m_recordBtn->setEnabled(true);
    m_stopRecordBtn->setEnabled(false);
    m_statusRecording->setText("REC: Off");
    m_statusRecording->setStyleSheet("");
}

void MainWindow::onLoadSession() {
    QString dir = QFileDialog::getExistingDirectory(this, "Open Investigation Session",
        QDir::homePath() + "/BTHL-SpiritBox/sessions");
    if (!dir.isEmpty()) {
        m_player->loadSession(dir);
    }
}

void MainWindow::onLoadWhisperModel() {
    QString modelPath = QFileDialog::getOpenFileName(this, "Load Whisper Model",
        QDir::homePath(), "Whisper Models (*.bin)");
    if (!modelPath.isEmpty()) {
        if (m_whisper->loadModel(modelPath)) {
            m_loadModelBtn->setText("Model Loaded");
            m_loadModelBtn->setEnabled(false);
            statusBar()->showMessage("We loaded the Whisper model successfully", 3000);
        }
    }
}

void MainWindow::onSweepStatusUpdated(const SweepStatus& status) {
    // We format the frequency display
    double freqMHz = status.currentFreqHz / 1e6;
    if (status.currentFreqHz < 1e6) {
        m_freqLabel->setText(QString("%1 kHz").arg(status.currentFreqHz / 1e3, 0, 'f', 1));
    } else {
        m_freqLabel->setText(QString("%1 MHz").arg(freqMHz, 0, 'f', 3));
    }
}

void MainWindow::onVoiceDetected(const VoiceDetectionEvent& event) {
    QString entry = QString("[%1s] VOICE DETECTED @ %2 MHz | Confidence: %3 | Energy: %4 dB")
        .arg(event.timestamp, 8, 'f', 2)
        .arg(event.frequencyHz / 1e6, 0, 'f', 3)
        .arg(event.confidence, 0, 'f', 3)
        .arg(event.energyDb, 0, 'f', 1);

    m_detectionLog->append(entry);
}

void MainWindow::onTranscriptionReady(const TranscriptionResult& result) {
    QString entry = QString("[%1s] @ %2 MHz: \"%3\" (prob: %4, %5ms)")
        .arg(result.timestamp, 8, 'f', 2)
        .arg(result.frequencyHz / 1e6, 0, 'f', 3)
        .arg(result.text)
        .arg(result.whisperProbability, 0, 'f', 3)
        .arg(result.processingTimeMs);

    m_transcriptionLog->append(entry);
}

void MainWindow::onCorrelatedEvent(const CorrelatedEvent& event) {
    QString entry = QString("*** CORRELATED EVENT #%1 ***\n"
                            "  Time: %2s | Score: %3 | Delta: %4ms\n"
                            "  EMF: %5 mG | Voice @ %6 MHz")
        .arg(m_correlator->correlatedEventCount())
        .arg(event.timestamp, 0, 'f', 2)
        .arg(event.correlationScore, 0, 'f', 3)
        .arg(event.timeDeltaMs, 0, 'f', 1)
        .arg(event.emfReading.emfMilligauss, 0, 'f', 2)
        .arg(event.voiceEvent.frequencyHz / 1e6, 0, 'f', 3);

    if (event.hasTranscription) {
        entry += QString("\n  Transcription: \"%1\"").arg(event.transcription.text);
    }

    m_detectionLog->append(entry);
    m_transcriptionLog->append(entry);
}

void MainWindow::onVADMetricsUpdated(float energy, float /*zcr*/,
                                      float /*flatness*/, float confidence) {
    m_statusVAD->setText(QString("VAD: %1dB / %2")
                         .arg(energy, 0, 'f', 1)
                         .arg(confidence, 0, 'f', 2));
}

void MainWindow::onEMFReading(const EMFReading& reading) {
    m_emfLabel->setText(QString("EMF: %1 mG").arg(reading.emfMilligauss, 0, 'f', 2));

    if (reading.isSpike) {
        m_emfLabel->setStyleSheet("QLabel { color: #ff0000; background-color: #1a1a2e; "
                                   "padding: 15px; border-radius: 8px; font-weight: bold; }");
    } else {
        m_emfLabel->setStyleSheet("QLabel { color: #ff6b6b; background-color: #1a1a2e; "
                                   "padding: 15px; border-radius: 8px; }");
    }
}

void MainWindow::onUpdateStatusBar() {
    if (m_sweepEngine->isSweeping()) {
        m_statusFreq->setText("Freq: Sweeping");
    }

    if (m_recorder->isRecording()) {
        m_statusRecording->setText("REC: ACTIVE");
    }
}

} // namespace bthl::spiritbox
