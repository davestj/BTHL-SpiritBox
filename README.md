<p align="center">
  <h1 align="center">BTHL-SpiritBox</h1>
  <p align="center">
    <strong>Paranormal Investigation Command Center</strong>
  </p>
  <p align="center">
    <em>Beyond The Horizon Labs — BTHLCorp</em>
  </p>
  <p align="center">
    <a href="#features">Features</a> •
    <a href="#build">Build</a> •
    <a href="#usage">Usage</a> •
    <a href="#whisper">Whisper AI</a> •
    <a href="#hardware">Hardware</a> •
    <a href="docs/USER_GUIDE.md">User Guide</a>
  </p>
</p>

---

## Overview

BTHL-SpiritBox is a multi-sensor paranormal audio detection and transcription system. It integrates SDR frequency sweeping with real-time audio demodulation, AI-powered voice detection, electromagnetic field correlation, and comprehensive session archival.

The spirit box technique rapidly sweeps radio frequencies, and our system automatically detects voice-like patterns in the demodulated audio, transcribes them using local AI (whisper.cpp), and correlates them with EMF sensor data to identify potential paranormal phenomena.

**Companion app to [BTHL-SpectraSentry Tactical Skanner](https://github.com/davestj/BTHL-SpectraSentry).**

---

## Features

### Signal Processing Pipeline
```
SDR (SoapySDR) → IQ Samples → Audio Demodulator (AM/FM/SSB)
    ├→ Speaker Output (PortAudio)
    ├→ Voice Activity Detector → Whisper AI Transcription
    ├→ EMF Correlator ← GQ EMF-390 Serial Reader
    └→ Session Recorder (WAV + JSON + CSV)
```

### Core Capabilities

| Feature | Description |
|---------|-------------|
| **SDR Frequency Sweep** | SoapySDR multi-device support with configurable profiles |
| **Audio Demodulation** | AM, NFM, WFM, USB, LSB modes with DC removal and decimation |
| **Voice Activity Detection** | Multi-feature analysis: RMS energy, zero-crossing rate, spectral flatness |
| **AI Transcription** | Local whisper.cpp speech-to-text (no cloud, no internet required) |
| **EMF Monitoring** | GQ EMF-390v2 serial protocol with spike detection |
| **Temporal Correlation** | 500ms sliding window matches EMF spikes with voice events |
| **Session Recording** | WAV audio + JSONL events + CSV EMF timeline + audio snippets |

### Visualization Widgets

| Widget | Description |
|--------|-------------|
| **Spectrum Waterfall** | Scrolling spectrogram with BTHL cosmic gradient |
| **Audio Waveform** | Real-time waveform + VU meter + VAD confidence bar |
| **EMF Timeline** | Scrolling time-series with spike markers and value labels |
| **Detection Log** | Color-coded filterable event log (voice/transcription/correlation/EMF) |

### Sweep Profiles

| Profile | Range | Steps | Modulation |
|---------|-------|-------|------------|
| FM Broadcast | 88-108 MHz | 101 | WFM |
| AM Broadcast | 530-1700 kHz | 118 | AM |
| VHF Low Band | 30-88 MHz | 58 | NFM |
| Full Spectrum | All bands | Variable | Mixed |

---

## Build

### Prerequisites
```bash
brew install qt@6 soapysdr soapyrtlsdr portaudio pkg-config cmake
```

### Optional: Whisper AI Transcription
```bash
# Clone whisper.cpp into lib/
git clone https://github.com/ggerganov/whisper.cpp.git lib/whisper.cpp

# Download a model
mkdir -p ~/BTHL-SpiritBox/models
curl -L -o ~/BTHL-SpiritBox/models/ggml-base.en.bin \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
```

### Compile
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew"
cmake --build . -j$(sysctl -n hw.ncpu)
```

### Run
```bash
./BTHL-SpiritBox
```

---

## Usage

### Quick Start

1. **Launch** — the app auto-detects your SDR and EMF-390
2. **Profile** is set to FM Broadcast by default (88-108 MHz)
3. Click **START SWEEP** — waterfall fills, audio plays
4. Voice detections appear in the Detection Log with confidence scores
5. EMF spikes correlate with voice events for high-confidence anomalies

### Loading a Whisper Model

1. Click **Load Model** in the Transcription dock
2. Browse to `~/BTHL-SpiritBox/models/ggml-base.en.bin`
3. Transcriptions appear automatically when voice is detected

### Session Recording

1. Click **Record** — creates timestamped session directory
2. All audio, events, and EMF data are archived
3. Click **Stop** to end recording
4. Sessions saved to `~/BTHL-SpiritBox/sessions/`

### Detection Log Filters

- **Voice** (green) — VAD confidence triggers
- **Transcription** (blue) — Whisper AI text results
- **Correlation** (gold) — EMF + voice coincidence events
- **EMF Spike** (red) — Significant electromagnetic deviations

---

## Hardware

### Supported Devices

| Device | Purpose | Connection |
|--------|---------|-----------|
| **RTL-SDR** (via SoapySDR) | RF frequency sweeping | USB (any SoapySDR-compatible device) |
| **GQ-EMF-390v2** | EMF/EF sensing | USB serial (CH340, VID 0x1A86) |

### SoapySDR Module Installation

```bash
# Required for RTL-SDR
brew install soapyrtlsdr

# Optional for other SDR devices
brew install soapyhackrf soapyremote
```

### Verify Device Detection
```bash
SoapySDRUtil --find
```

---

## Voice Detection Science

### Multi-Feature VAD Algorithm

The Voice Activity Detector uses three audio features with weighted scoring:

| Feature | Weight | Voice Characteristic |
|---------|--------|---------------------|
| RMS Energy | 30% | Voice is louder than background noise |
| Zero-Crossing Rate | 30% | Voice: 0.02-0.15, noise: higher |
| Spectral Flatness | 40% | Voice has spectral peaks, noise is flat |

**Confidence Formula:**
```
confidence = 0.30 × energy + 0.30 × zcr + 0.40 × flatness
```

Triggers when `confidence > 0.5` and `duration > 100ms`.

### EMF Correlation

The EMF Correlator uses a 500ms sliding window to detect temporal coincidence between voice detection events and electromagnetic field spikes. Correlation score factors:
- **Temporal proximity** — how close in time the events occurred
- **EMF spike strength** — deviation from rolling baseline
- **Combined confidence** — weighted product of VAD and EMF scores

---

## Architecture

```
bthl-spiritbox/
├── src/
│   ├── core/              SweepEngine, AudioDemodulator, SweepProfile
│   ├── audio/             AudioOutputManager, AudioRingBuffer (lock-free SPSC)
│   ├── detection/         VoiceActivityDetector, WhisperTranscriber
│   ├── emf/               EMFSerialReader, EMFCorrelator
│   ├── hub/               SensorHub (multi-sensor orchestration)
│   ├── sensors/           SensorDevice, USBDeviceManager
│   ├── session/           SessionRecorder, SessionPlayer
│   ├── ui/                MainWindow + 4 visualization widgets
│   └── main.cpp
├── profiles/              Sweep profile JSON files
├── docs/                  User guide, architecture documentation
├── lib/whisper.cpp/       whisper.cpp submodule (optional)
└── CMakeLists.txt
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed module descriptions and thread model.

---

## Session Output Format

Each recording session creates a directory with:

```
sessions/2026-04-12_20-30-15/
├── session.json           Session metadata (ID, timestamp, devices)
├── audio_sweep.wav        Complete audio recording (16-bit PCM)
├── events.jsonl           All detection events (newline-delimited JSON)
├── emf_timeline.csv       EMF readings with spike flags
└── audio_snippets/
    ├── voice_001.wav      Individual VAD audio clips
    ├── voice_002.wav
    └── ...
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [User Guide](docs/USER_GUIDE.md) | Complete usage instructions and best practices |
| [Architecture](docs/ARCHITECTURE.md) | System design, data flow, and module descriptions |
| [CLAUDE.md](CLAUDE.md) | Development guide for Claude Code |

---

## Integration with SpectraSentry

BTHL-SpiritBox is designed as a companion to [BTHL-SpectraSentry](https://github.com/davestj/BTHL-SpectraSentry). Both apps share the GQ EMF-390 and RTL-SDR hardware (one app at a time). Future integration via IPC bridge will enable cross-app anomaly correlation.

---

## License

Copyright (c) 2026 BTHLCorp — Beyond The Horizon Labs. All rights reserved.

## Credits

- **David St. John** — Project Lead, BTHLCorp
- **Claude Opus 4.6** — Architecture & Code Generation
- Open source: Qt 6, SoapySDR, PortAudio, whisper.cpp, yaml-cpp
