# CHANGELOG

All notable changes to BTHL-SpiritBox will be documented in this file.

## [1.0.0] - 2026-03-30

### Added
- Initial project scaffold with full v1 architecture
- SweepEngine with SoapySDR multi-device abstraction for frequency hopping
- SweepProfile with JSON serialization and factory presets (AM, FM, VHF, Full Spectrum)
- AudioDemodulator with AM envelope, NFM/WFM discriminator, USB/LSB demodulation
- AudioRingBuffer lock-free SPSC ring buffer for real-time audio streaming
- AudioOutputManager with PortAudio callback-driven playback
- VoiceActivityDetector with energy, ZCR, and spectral flatness features
- WhisperTranscriber with async whisper.cpp integration for local speech-to-text
- EMFSerialReader for GQ EMF-390 USB serial communication
- EMFCorrelator for temporal correlation between EMF spikes and voice events
- SessionRecorder with WAV audio recording and JSON event stream logging
- SessionPlayer foundation for post-investigation session playback
- MainWindow with dock-based UI layout and full subsystem wiring
- Dark investigation theme optimized for low-light field conditions
- Comprehensive documentation with first-person plural comment style
- CMake build system with conditional whisper.cpp support

### Phase 2 Planned
- SweepVisualizerWidget with Qt Charts waterfall spectrum display
- EMFTimelineWidget with scrolling EMF chart and spike markers
- AudioWaveformWidget with OpenGL waveform rendering
- DetectionLogWidget with filterable event log and event markers
- TinySA4 integration for spectrum cross-validation
- Session playback with synchronized audio and event timeline
- Custom sweep profile editor UI
- Export to CSV/HTML investigation reports
