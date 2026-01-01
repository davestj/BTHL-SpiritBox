# BTHL-SpiritBox Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                    MainWindow (Qt6)                       │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  │ Spectrum  │ │  Audio   │ │   EMF    │ │Detection │   │
│  │Waterfall  │ │ Waveform │ │ Timeline │ │   Log    │   │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘   │
│       │             │            │             │          │
├───────┼─────────────┼────────────┼─────────────┼──────────┤
│       │    Signal Processing Pipeline         │          │
│  ┌────▼─────┐  ┌────▼──────┐  ┌──▼────┐  ┌───▼──────┐  │
│  │  Sweep   │  │  Audio    │  │  VAD  │  │ Whisper  │  │
│  │  Engine  │──│ Demod     │──│       │──│Transcribe│  │
│  │(SoapySDR)│  │(AM/FM/SSB)│  │       │  │(.cpp)    │  │
│  └────┬─────┘  └────┬──────┘  └──┬────┘  └───┬──────┘  │
│       │             │            │             │          │
│  ┌────▼─────┐  ┌────▼──────┐  ┌──▼────────────▼──────┐  │
│  │   SDR    │  │ PortAudio │  │    EMF Correlator    │  │
│  │ Hardware │  │  Output   │  │  (500ms window)      │  │
│  └──────────┘  └───────────┘  └──────────┬───────────┘  │
│                                           │              │
│                              ┌────────────▼──────────┐   │
│                              │   Session Recorder    │   │
│                              │ WAV + JSONL + CSV     │   │
│                              └───────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │           USB Device Manager                      │    │
│  │   SDR enumerate │ EMF detect │ Audio enum         │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```

## Thread Model

| Thread | Purpose |
|--------|---------|
| Main (Qt Event Loop) | UI rendering, signal/slot dispatch |
| SweepEngine Worker | SDR IQ capture, frequency hopping |
| PortAudio Callback | Real-time audio output |
| Whisper Inference | AI transcription (QtConcurrent) |
| USB Monitor | 2-second polling for device changes |

## Module Descriptions

### SweepEngine (core/)
- Manages SoapySDR device lifecycle
- Executes frequency sweep profiles (frequency list + dwell times)
- Emits IQ samples and signal power measurements
- Thread-safe with QMutex for device access

### AudioDemodulator (core/)
- AM envelope detection
- NFM/WFM discriminator demodulation
- USB/LSB sideband extraction
- DC removal, decimation, normalization

### VoiceActivityDetector (detection/)
- Multi-feature analysis: RMS energy, zero-crossing rate, spectral flatness
- Configurable thresholds and minimum duration
- Captures audio snippets for Whisper transcription
- Emits VoiceDetectionEvent with confidence score

### WhisperTranscriber (detection/)
- Loads GGML whisper.cpp models
- Async inference via QtConcurrent
- Conditional compilation (#if WHISPER_AVAILABLE)
- Returns TranscriptionResult with text + confidence

### EMFSerialReader (emf/)
- GQ EMF-390 protocol: 4-byte big-endian IEEE 754 float
- Auto-detection via CH340 vendor ID (0x1A86)
- Spike detection with configurable threshold
- Rolling baseline calculation (100-sample window)

### EMFCorrelator (emf/)
- Sliding window temporal correlation (500 ms)
- Matches EMF spikes with voice detection events
- Generates CorrelatedEvent with correlation score
- Attaches Whisper transcription when available

### SessionRecorder (session/)
- WAV file recording (16-bit PCM, RIFF headers)
- JSONL event log (one JSON object per line)
- CSV EMF timeline with spike flagging
- Audio snippet archival per voice detection
- Session metadata (ID, timestamp, platform)
