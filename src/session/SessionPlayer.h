/**
 * @file src/session/SessionPlayer.h
 * @title SessionPlayer - Post-Investigation Session Playback
 * @author David St John (davestj)
 * @date 2026-04-12
 * @purpose We provide playback of recorded investigation sessions, synchronized
 *          with audio, event timeline, EMF data, and transcription results.
 * @reason Investigators need to review sessions after the fact, scrubbing through
 *         correlated events and replaying audio at specific timestamps.
 *
 * CHANGELOG:
 * 2026-04-12 - Initial stub implementation to unblock build
 */

#ifndef BTHL_SPIRITBOX_SESSION_PLAYER_H
#define BTHL_SPIRITBOX_SESSION_PLAYER_H

#include <QObject>
#include <QString>
#include <string>

namespace bthl::spiritbox {

/**
 * @class SessionPlayer
 * @purpose We load and play back a recorded investigation session, providing
 *          synchronized audio playback with event timeline navigation.
 */
class SessionPlayer : public QObject {
    Q_OBJECT

public:
    explicit SessionPlayer(QObject* parent = nullptr);
    ~SessionPlayer() override = default;

    /// We load a session from the given directory
    bool loadSession(const QString& sessionDir);

    /// We check if a session is loaded
    bool isLoaded() const { return loaded_; }

    /// We return the session directory path
    QString sessionDir() const { return session_dir_; }

signals:
    /// We emit when a session is successfully loaded
    void sessionLoaded(const QString& sessionDir);

    /// We emit on load error
    void loadError(const QString& error);

private:
    bool loaded_{false};
    QString session_dir_;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_SESSION_PLAYER_H
