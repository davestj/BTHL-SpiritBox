/**
 * @file src/net/UpdateChecker.h
 * @title UpdateChecker - In-app update checker
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We query bthlcorp.com for the latest released version and, when a newer build is
 *          available, surface the download so the user can update from inside the app.
 *
 * The server endpoint (updates.php) returns JSON:
 *   { "version": "1.1.2", "pkg_url": "https://bthlcorp.com/spiritbox/downloads/bthl-spiritbox-1.1.2.pkg",
 *     "notes": "…", "min_os": "12.0" }
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#ifndef BTHL_SPIRITBOX_UPDATE_CHECKER_H
#define BTHL_SPIRITBOX_UPDATE_CHECKER_H

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE

namespace bthl::spiritbox {

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    /// We fetch updates.php and compare against the running version (e.g. "1.0.0").
    void checkForUpdates(const QString& currentVersion);

    /// Semantic-ish version compare: returns true if `candidate` > `current`.
    static bool isNewer(const QString& candidate, const QString& current);

signals:
    void updateAvailable(const QString& version, const QString& pkgUrl, const QString& notes);
    void upToDate(const QString& currentVersion);
    void checkFailed(const QString& reason);

private:
    QNetworkAccessManager* m_net{nullptr};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_UPDATE_CHECKER_H
