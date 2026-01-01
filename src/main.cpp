/**
 * @file src/main.cpp
 * @title BTHL-SpiritBox Application Entry Point
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We initialize the Qt application and launch the MainWindow which
 *          owns all subsystem lifecycle and signal wiring.
 *
 * CHANGELOG:
 * 2026-03-30 - Initial scaffold
 * 2026-03-31 - Expanded with standalone subsystem wiring
 * 2026-04-12 - Simplified: MainWindow now owns all subsystems (no duplicates)
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

#include "ui/MainWindow.h"

using namespace bthl::spiritbox;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("BTHL-SpiritBox");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Beyond The Horizon Labs");
    app.setOrganizationDomain("beyondthehorizonlabs.com");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "BTHL-SpiritBox - Paranormal Investigation Command Center\n"
        "Multi-sensor anomaly detection, SDR spirit box, and AI transcription"
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    qInfo() << "==================================================";
    qInfo() << "  BTHL-SpiritBox v1.0.0";
    qInfo() << "  Paranormal Investigation Command Center";
    qInfo() << "  Beyond The Horizon Labs";
    qInfo() << "  (C) 2026 David St John";
    qInfo() << "==================================================";

    /// We launch the MainWindow — it creates and owns all subsystems
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
