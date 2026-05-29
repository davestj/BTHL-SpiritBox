/**
 * @file src/emf/EMFSerialReader.h
 * @title EMFSerialReader - GQ EMF-390 Serial Interface
 * @author David St John (davestj)
 * @date 2026-03-30
 * @purpose We read EMF measurements from the GQ EMF-390 meter over its USB serial interface.
 *          The reader parses the device protocol and emits structured EMF readings.
 * @reason Correlating EMF spikes with audio detections is a key differentiator for our
 *         spirit box. The GQ EMF-390 provides calibrated EMF measurements we can timestamp
 *         and align with sweep and voice detection events.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial implementation with GQ EMF-390 protocol parsing
 */

#ifndef BTHL_SPIRITBOX_EMF_SERIAL_READER_H
#define BTHL_SPIRITBOX_EMF_SERIAL_READER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QElapsedTimer>
#include <vector>

namespace bthl::spiritbox {

/**
 * @struct EMFReading
 * @purpose We store a single EMF measurement with metadata
 */
struct EMFReading {
    double timestamp;       ///< Session time in seconds
    double emfMilligauss;   ///< EMF strength in milligauss (the value we actually read)
    double efVm;            ///< Electric field in V/m — NaN when not measured by this firmware
    double rfMwCm2;         ///< RF power density in mW/cm^2 — NaN when not measured
    bool isSpike;           ///< Whether we consider this an anomalous spike
};

/**
 * @struct EmfCapabilities
 * @purpose We capture what the connected EMF meter actually is and reports, probed from the
 *          device (firmware version) — not assumed. Used by the Device Capabilities panel.
 */
struct EmfCapabilities {
    bool valid{false};          ///< True once connected and probed
    QString portName;           ///< Serial port the meter is on
    QString firmwareVersion;    ///< Firmware string from <GETVER>>
    int pollingIntervalMs{0};   ///< Active polling cadence
    bool readsEmf{true};        ///< We read magnetic field (milligauss)
    bool readsEf{false};        ///< Electric field reading available/parsed
    bool readsRf{false};        ///< RF power-density reading available/parsed
};

/**
 * @class EMFSerialReader
 * @purpose We manage the serial connection to the GQ EMF-390 and continuously
 *          read EMF measurements for correlation with audio events.
 */
class EMFSerialReader : public QObject {
    Q_OBJECT

public:
    explicit EMFSerialReader(QObject* parent = nullptr);
    ~EMFSerialReader() override;

    /**
     * @brief We enumerate available serial ports that might be GQ EMF-390 devices
     * @return List of serial port info matching known GQ device identifiers
     */
    static QList<QSerialPortInfo> enumerateDevices();

    /**
     * @brief We open a serial connection to the EMF meter
     * @param portName Serial port name (e.g., /dev/tty.usbserial-XXXX)
     * @param baudRate Baud rate (GQ EMF-390 typically uses 115200)
     * @return true if we successfully connected
     */
    bool connectDevice(const QString& portName, int baudRate = 115200);

    /**
     * @brief We disconnect from the EMF meter
     */
    void disconnectDevice();

    /**
     * @brief We check if we are connected to a device
     */
    [[nodiscard]] bool isConnected() const;

    /**
     * @brief We set the polling interval for EMF readings
     * @param intervalMs Polling interval in milliseconds (default 100ms = 10 readings/sec)
     */
    void setPollingInterval(int intervalMs);

    /**
     * @brief We start continuous EMF reading
     */
    void startReading();

    /**
     * @brief We stop continuous EMF reading
     */
    void stopReading();

    /**
     * @brief We set the spike detection threshold in milligauss
     *        Readings above this value are flagged as spikes
     * @param threshold Threshold in milligauss
     */
    void setSpikeThreshold(double threshold);

    /**
     * @brief We get the current spike threshold
     */
    [[nodiscard]] double spikeThreshold() const;

    /**
     * @brief We get the most recent EMF reading
     */
    [[nodiscard]] EMFReading lastReading() const;

    /**
     * @brief We get the baseline EMF level (rolling average)
     */
    [[nodiscard]] double baselineMilligauss() const;

    /**
     * @brief We return what we know about the connected meter (firmware, sensors, polling).
     */
    [[nodiscard]] EmfCapabilities capabilities() const;

signals:
    /**
     * @brief We emit each new EMF reading
     */
    void readingReceived(const EMFReading& reading);

    /**
     * @brief We emit when an EMF spike is detected above threshold
     */
    void spikeDetected(const EMFReading& reading);

    /**
     * @brief We emit connection state changes
     */
    void connectionStateChanged(bool connected);

    /**
     * @brief We emit the meter's probed capabilities once we have its firmware version.
     */
    void capabilitiesProbed(const EmfCapabilities& caps);

    /**
     * @brief We emit errors
     */
    void errorOccurred(const QString& message);

private slots:
    void onDataReady();
    void onPollTimer();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    /**
     * @brief We parse the GQ EMF-390 data protocol response
     * @param data Raw bytes received from the device
     */
    void parseResponse(const QByteArray& data);

    /**
     * @brief We send a command to the GQ EMF-390
     * @param command The protocol command string
     */
    void sendCommand(const QByteArray& command);

    /**
     * @brief We update the rolling baseline average
     */
    void updateBaseline(double emfValue);

    /// We track which command's response we are expecting so a version reply is never
    /// misread as an EMF float (and vice-versa).
    enum class Expecting { None, Version, Emf };

    QSerialPort* m_serialPort{nullptr};
    QTimer* m_pollTimer{nullptr};
    QElapsedTimer m_sessionTimer;

    EMFReading m_lastReading{};
    double m_spikeThreshold{5.0};   // 5 milligauss default spike threshold
    double m_baseline{0.0};
    std::vector<double> m_baselineWindow;
    static constexpr size_t BASELINE_WINDOW_SIZE = 100;

    QByteArray m_readBuffer;
    bool m_reading{false};

    Expecting m_expecting{Expecting::None};
    EmfCapabilities m_capabilities;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_EMF_SERIAL_READER_H
