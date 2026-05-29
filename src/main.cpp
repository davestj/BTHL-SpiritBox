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
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QSplashScreen>
#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

#include "ui/MainWindow.h"

using namespace bthl::spiritbox;

int main(int argc, char* argv[]) {
    // QtWebEngine (the in-app documentation browser) requires shared OpenGL contexts, and the
    // attribute must be set before the QApplication is constructed.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setApplicationName("BTHL-SpiritBox");
    app.setApplicationVersion(QStringLiteral(BTHL_SPIRITBOX_VERSION));
    app.setOrganizationName("Beyond The Horizon Labs");
    app.setOrganizationDomain("beyondthehorizonlabs.com");
    app.setWindowIcon(QIcon(":/icons/icon-512.png"));

    /// We enforce a single running instance. Two instances would open the same
    /// RTL-SDR concurrently, corrupting retune (random PLL-lock failures) and
    /// triggering stream OVERFLOWs — the device cannot be shared (see CLAUDE.md).
    QLockFile lockFile(QDir::temp().absoluteFilePath("bthl-spiritbox.lock"));
    // We allow QLockFile to reclaim a lock left by a crashed instance: with a non-zero stale
    // time it checks the recorded PID and removes the lock if that process is gone, so a hard
    // crash never blocks future launches forever.
    lockFile.setStaleLockTime(30000);
    if (!lockFile.tryLock(100)) {
        qCritical() << "BTHL-SpiritBox: Another instance is already running — refusing to start.";
        QMessageBox::critical(nullptr, "BTHL-SpiritBox Already Running",
            "Another instance of BTHL-SpiritBox is already running.\n\n"
            "Only one instance may run at a time because the RTL-SDR device cannot "
            "be shared — a second instance would corrupt frequency tuning and cause "
            "audio dropouts.\n\nPlease close the existing window first.");
        return 1;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "BTHL-SpiritBox - Paranormal Investigation Command Center\n"
        "Multi-sensor anomaly detection, SDR spirit box, and AI transcription"
    );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    qInfo() << "==================================================";
    qInfo() << "  BTHL-SpiritBox v" BTHL_SPIRITBOX_VERSION;
    qInfo() << "  Paranormal Investigation Command Center";
    qInfo() << "  Beyond The Horizon Labs";
    qInfo() << "  (C) 2026 David St John";
    qInfo() << "==================================================";

    /// We show a branded loading splash for a visible dwell BEFORE we build the main window, so
    /// nothing (no sweep, no audio) starts underneath it — the splash is the only thing happening,
    /// and its status line animates so it never looks frozen. The pixmap is compiled into the
    /// binary; if for any reason it is missing we fall back to a plain splash so we never show a
    /// blank/invisible window.
    const QColor splashInk("#cdd6f0");
    const int splashAlign = Qt::AlignBottom | Qt::AlignHCenter;
    constexpr int kSplashMinMs = 6000;

    QPixmap splashPix(":/branding/splash.png");
    if (splashPix.isNull()) {
        splashPix = QPixmap(900, 560);
        splashPix.fill(QColor("#0d1226"));
    }
    QSplashScreen splash(splashPix);
    splash.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    splash.show();
    splash.raise();
    app.processEvents();

    QElapsedTimer splashTimer;
    splashTimer.start();

    // We animate the status line through the dwell so the splash visibly "lives."
    struct Stage { int atMs; const char* msg; };
    const Stage stages[] = {
        {0,    "Initializing subsystems…"},
        {1500, "Detecting SDR, EMF, and microphone…"},
        {3000, "Loading audio + detection pipeline…"},
        {4500, "Preparing investigation console…"},
        {5600, "Ready."},
    };
    int nextStage = 0;
    while (splashTimer.elapsed() < kSplashMinMs) {
        if (nextStage < 5 && splashTimer.elapsed() >= stages[nextStage].atMs) {
            splash.showMessage(stages[nextStage].msg, splashAlign, splashInk);
            ++nextStage;
        }
        app.processEvents(QEventLoop::AllEvents, 40);
        QThread::msleep(20);
    }

    /// Only now do we build the window and let it auto-connect hardware / start the sweep, so
    /// audio never plays while the user is still staring at the splash.
    MainWindow mainWindow;
    mainWindow.show();
    splash.finish(&mainWindow);

    return app.exec();
}
