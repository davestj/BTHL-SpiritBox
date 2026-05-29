/**
 * @file src/net/UpdateChecker.cpp
 * @author David St John (davestj)
 * @date 2026-05-29
 */

#include "net/UpdateChecker.h"
#include "net/BthlEndpoints.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QList>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

namespace bthl::spiritbox {

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

bool UpdateChecker::isNewer(const QString& candidate, const QString& current) {
    // We compare dotted numeric versions field-by-field (1.10.0 > 1.9.9). Non-numeric suffixes
    // are ignored for ordering. Missing fields are treated as 0.
    const auto parts = [](const QString& v) {
        QList<int> out;
        for (const QString& seg : v.split('.')) {
            out << seg.section(QRegularExpression("[^0-9]"), 0, 0).toInt();
        }
        return out;
    };
    const QList<int> a = parts(candidate);
    const QList<int> b = parts(current);
    const int n = std::max(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        const int ai = i < a.size() ? a[i] : 0;
        const int bi = i < b.size() ? b[i] : 0;
        if (ai != bi) return ai > bi;
    }
    return false;  // equal → not newer
}

void UpdateChecker::checkForUpdates(const QString& currentVersion) {
    QNetworkRequest req{QUrl(endpoints::kUpdateCheck)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("BTHL-SpiritBox/%1").arg(currentVersion));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, currentVersion]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit checkFailed("We received an unexpected response from the update server.");
            return;
        }
        const QJsonObject obj = doc.object();
        const QString latest = obj.value("version").toString();
        const QString pkgUrl = obj.value("pkg_url").toString();
        const QString notes = obj.value("notes").toString();
        if (latest.isEmpty()) {
            emit checkFailed("The update server did not report a version.");
            return;
        }
        if (isNewer(latest, currentVersion) && !pkgUrl.isEmpty()) {
            emit updateAvailable(latest, pkgUrl, notes);
        } else {
            emit upToDate(currentVersion);
        }
    });
}

} // namespace bthl::spiritbox
