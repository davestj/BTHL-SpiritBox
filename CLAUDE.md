# BTHL-SpiritBox — Claude Code Guide

## Project Overview

Paranormal investigation command center with SDR frequency sweeping, real-time audio demodulation, AI voice detection (whisper.cpp), GQ EMF-390 correlation, and multi-sensor session archival. Companion app to BTHL-SpectraSentry Tactical Skanner.

"SpiritBox" refers to the paranormal investigation technique of rapidly sweeping radio frequencies to detect Electronic Voice Phenomena (EVP).

## Build System

**CMake** (C++20 required).

```bash
# Install dependencies
brew install qt@6 soapysdr soapyrtlsdr portaudio pkg-config

# Optional: whisper.cpp for AI transcription
git clone https://github.com/ggerganov/whisper.cpp.git lib/whisper.cpp

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew"
cmake --build . -j$(sysctl -n hw.ncpu)
```

## Architecture

```
bthl-spiritbox/
├── src/
│   ├── core/           SweepEngine, AudioDemodulator, SweepProfile
│   ├── audio/          AudioOutputManager, AudioRingBuffer
│   ├── detection/      VoiceActivityDetector, WhisperTranscriber
│   ├── emf/            EMFSerialReader, EMFCorrelator
│   ├── hub/            SensorHub (multi-sensor orchestration)
│   ├── sensors/        SensorDevice, USBDeviceManager
│   ├── session/        SessionRecorder, SessionPlayer
│   ├── ui/             MainWindow + 4 visualization widgets
│   └── main.cpp
├── profiles/           Sweep profile JSON files
├── lib/whisper.cpp/    whisper.cpp submodule (optional)
└── CMakeLists.txt
```

## Data Flow Pipeline

```
SweepEngine (SoapySDR) → IQ Samples
    ↓
AudioDemodulator (AM/FM/SSB) → PCM Audio
    ├→ AudioOutputManager (PortAudio speakers)
    ├→ VoiceActivityDetector → WhisperTranscriber (local AI)
    └→ SessionRecorder (WAV + JSONL)

EMFSerialReader (GQ-390) → EMFCorrelator ← VoiceDetectionEvent
    ↓
CorrelatedEvent → SessionRecorder
```

## Supported Hardware

| Device | Protocol | Purpose |
|--------|----------|---------|
| RTL-SDR (via SoapySDR) | USB bulk | RF frequency sweeping |
| GQ-EMF-390v2 | USB serial (CH340, VID 0x1A86) | EMF/EF/Gyro sensing |

## Key Dependencies

Qt6 (Widgets, Multimedia, SerialPort, Charts, Concurrent), SoapySDR, PortAudio, whisper.cpp (optional)

## UI Visualization Widgets

- **SweepVisualizerWidget** — Scrolling spectrum waterfall (QPainter)
- **AudioWaveformWidget** — Real-time waveform + VU meter + VAD confidence bar
- **EMFTimelineWidget** — Scrolling EMF time-series with spike markers
- **DetectionLogWidget** — Filterable event log (voice/transcription/correlation/EMF)

## Sweep Profiles

| Profile | Range | Modulation |
|---------|-------|------------|
| AM Broadcast | 530-1700 kHz | AM |
| FM Broadcast | 88-108 MHz | WFM |
| VHF Low Band | 30-88 MHz | NFM |
| Full Spectrum | All bands | Mixed |

## Coding Conventions

- **Namespace**: `bthl::spiritbox`
- **Comments**: "We" collaborative first-person plural
- **File naming**: CamelCase (Qt convention)
- **Threading**: QThread + QMutex + std::atomic

## Do NOT

- Switch from CMake to another build system
- Remove whisper.cpp conditional compilation
- Share the SDR device with SpectraSentry simultaneously
