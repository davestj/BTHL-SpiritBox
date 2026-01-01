# BTHL-SpiritBox User Guide

## Getting Started

### 1. Launch the Application
```bash
cd build
./BTHL-SpiritBox
```

The app auto-detects your SDR device and EMF-390 on startup. If devices are connected, they will appear in the Controls dock.

### 2. Understanding the Layout

The main window has 5 areas:

| Area | Location | Purpose |
|------|----------|---------|
| **Frequency Display** | Center top | Shows current tuned frequency |
| **Spectrum Waterfall** | Center middle | Scrolling spectrogram of signal power |
| **Audio Waveform** | Center bottom | Real-time audio + VU meter + VAD confidence |
| **Controls Dock** | Left side | SDR device, profile, sweep controls |
| **Detection Log** | Bottom | Voice, transcription, EMF, correlation events |
| **EMF Monitor** | Right side | Current EMF reading + timeline chart |
| **Transcription Dock** | Right side | Whisper AI transcription results |

### 3. Starting a Sweep Session

1. The app auto-connects to your SDR on startup
2. Select a sweep profile (FM Broadcast is default for RTL-SDR)
3. Click **START SWEEP** in the toolbar
4. Audio plays through speakers automatically
5. The waterfall fills with spectrum data
6. Voice detections appear in the Detection Log

### 4. Loading a Whisper Model

For AI speech-to-text transcription:

1. Download a model:
   ```bash
   curl -L -o ~/BTHL-SpiritBox/models/ggml-base.en.bin \
     https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
   ```
2. In the app, click **Load Model** in the Transcription dock
3. Browse to `~/BTHL-SpiritBox/models/ggml-base.en.bin`
4. Status changes to "Model Loaded"

Available models (smallest → largest):
| Model | Size | Speed | Quality |
|-------|------|-------|---------|
| tiny.en | 75 MB | Fastest | Basic |
| base.en | 142 MB | Fast | Good |
| small.en | 466 MB | Medium | Better |
| medium.en | 1.5 GB | Slow | Best |

### 5. EMF Monitoring

1. Connect the GQ EMF-390v2 via USB
2. The app auto-detects the device (CH340 USB serial)
3. EMF readings appear in the EMF Monitor dock
4. The timeline chart shows rolling EMF history with spike markers
5. Red spikes indicate anomalous EMF fluctuations

### 6. Session Recording

1. Click **Record** in the toolbar
2. A session directory is created with:
   - `session.json` — Session metadata
   - `audio_sweep.wav` — Complete audio recording
   - `events.jsonl` — All detection events
   - `emf_timeline.csv` — EMF data with spike flags
   - `audio_snippets/` — Individual WAV clips of voice detections
3. Click **Stop** to end recording

### 7. Detection Log Filters

The Detection Log has checkboxes to filter event types:
- **Voice** (green) — Voice activity detection triggers
- **Transcription** (blue) — Whisper AI transcription results
- **Correlation** (gold) — EMF + voice coincidence events
- **EMF Spike** (red) — Significant EMF deviations

---

## Sweep Profiles

### AM Broadcast (530-1700 kHz)
- **Note:** Requires a tuner that covers below 52 MHz. The RTL-SDR E4000 tuner minimum is 52 MHz, so AM band is unreachable with this hardware.

### FM Broadcast (88-108 MHz) — Recommended
- 101 frequency steps, 200 kHz spacing
- Wideband FM demodulation
- 48 kHz audio sample rate
- 30 ms dwell time per step

### VHF Low Band (30-88 MHz)
- Public safety, aviation, marine frequencies
- Narrowband FM demodulation

### Full Spectrum
- Covers AM + VHF + FM in sequence
- Longest sweep cycle time

---

## Understanding Voice Detection

### How VAD Works

The Voice Activity Detector (VAD) analyzes demodulated audio using three features:

1. **RMS Energy** — Signal power level (voice is louder than noise)
2. **Zero-Crossing Rate** — Rate of sign changes (voice: 0.02-0.15, noise: higher)
3. **Spectral Flatness** — How "white" the spectrum is (voice < noise)

**Confidence Score:**
```
confidence = 0.30 × energy_score + 0.30 × zcr_score + 0.40 × flatness_score
```

Voice is flagged when confidence > 0.5 and duration > 100 ms.

### How EMF Correlation Works

The EMF Correlator watches for temporal coincidence between voice detection events and EMF spikes within a 500 ms sliding window. A correlated event suggests that an electromagnetic disturbance occurred simultaneously with a potential voice phenomenon — the strongest indicator of anomalous activity.

---

## Troubleshooting

### SDR Not Detected
```bash
# Check if SoapySDR sees your device
SoapySDRUtil --find

# Install the RTL-SDR driver module if missing
brew install soapyrtlsdr
```

### EMF-390 Not Detected
- Ensure the CH340 USB driver is working
- Check `System Settings > Privacy > USB` permissions
- The device appears as `/dev/cu.usbserial-*`

### No Audio Output
- Check macOS Sound settings — ensure output device is correct
- PortAudio initializes at the profile's audio sample rate (48 kHz for FM)

### Whisper Model Not Loading
- Ensure the `.bin` file is a valid GGML whisper model
- Download from: `https://huggingface.co/ggerganov/whisper.cpp/tree/main`
- The app needs the `lib/whisper.cpp` submodule compiled for transcription to work

---

## Investigation Best Practices

1. **Environment Setup**
   - Record in a quiet location with minimal RF interference
   - Turn off phones, WiFi routers, and other electronics nearby
   - Document the location and time of each session

2. **Sweep Settings**
   - Use FM Broadcast for initial sweeps (strongest signals)
   - Switch to VHF Low for more obscure frequency ranges
   - Increase dwell time (50-100 ms) for better audio quality

3. **EVP Technique**
   - Ask clear questions with 10-15 second pauses
   - Speak loudly so your questions are distinguishable from responses
   - Mark anomalous moments manually in addition to auto-detection

4. **Multi-Sensor Correlation**
   - Run EMF monitoring alongside audio sweep
   - Correlated events (EMF + voice) are highest confidence
   - Review the detection log timeline after each session

5. **Session Review**
   - Load recorded sessions for post-investigation analysis
   - Listen to audio snippets flagged as voice detections
   - Cross-reference EMF timeline with detection timestamps
