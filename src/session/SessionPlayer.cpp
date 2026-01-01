/**
 * @file src/session/SessionPlayer.cpp
 * @title SessionPlayer - Post-Investigation Session Playback Implementation
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We implement session loading and playback for post-investigation review.
 *
 * CHANGELOG:
 * 2026-04-12 - Initial stub to unblock compilation. Full playback in Phase 2.
 */

#include "session/SessionPlayer.h"
#include <QDir>
#include <QFile>
#include <QDebug>

namespace bthl::spiritbox {

SessionPlayer::SessionPlayer(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[SessionPlayer] Session player created (Phase 2 — playback pending)";
}

bool SessionPlayer::loadSession(const QString& sessionDir)
{
    QDir dir(sessionDir);
    if (!dir.exists()) {
        emit loadError("Session directory does not exist: " + sessionDir);
        return false;
    }

    /// We verify the session contains expected files
    bool hasSessionJson = QFile::exists(dir.filePath("session.json"));
    bool hasEvents = QFile::exists(dir.filePath("events.jsonl"));

    if (!hasSessionJson) {
        emit loadError("Missing session.json in " + sessionDir);
        return false;
    }

    session_dir_ = sessionDir;
    loaded_ = true;

    qDebug() << "[SessionPlayer] Loaded session from:" << sessionDir
             << "| events.jsonl:" << hasEvents;

    emit sessionLoaded(sessionDir);
    return true;
}

} // namespace bthl::spiritbox
