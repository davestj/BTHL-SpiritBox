/**
 * @file src/net/ModelManager.cpp
 * @author David St John (davestj)
 * @date 2026-05-29
 */

#include "net/ModelManager.h"
#include "net/BthlEndpoints.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>

namespace bthl::spiritbox {

ModelManager::ModelManager(QObject* parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

bool ModelManager::isInstalled(const QString& file) const {
    if (m_modelsDir.isEmpty()) return false;
    return QFileInfo::exists(m_modelsDir + "/" + file);
}

void ModelManager::fetchCatalog() {
    QNetworkRequest req{QUrl(endpoints::kModelCatalog)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit catalogFailed(reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit catalogFailed("The model catalog response was not valid JSON.");
            return;
        }
        QList<ModelEntry> models;
        for (const QJsonValue& v : doc.object().value("models").toArray()) {
            const QJsonObject o = v.toObject();
            ModelEntry e;
            e.name = o.value("name").toString();
            e.file = o.value("file").toString();
            e.url = o.value("url").toString();
            e.description = o.value("description").toString();
            e.sizeMb = o.value("size_mb").toInt();
            e.bundled = o.value("bundled").toBool();
            e.installed = isInstalled(e.file);
            if (!e.file.isEmpty()) models << e;
        }
        emit catalogReady(models);
    });
}

void ModelManager::downloadModel(const ModelEntry& entry) {
    if (m_activeReply) {
        emit downloadFailed(entry.file, "Another download is already in progress.");
        return;
    }
    if (m_modelsDir.isEmpty() || !QDir().mkpath(m_modelsDir)) {
        emit downloadFailed(entry.file, "We could not access the models folder.");
        return;
    }
    // We download to a .part file and rename on success so a partial download never looks valid.
    const QString partPath = m_modelsDir + "/" + entry.file + ".part";
    m_activeFile = new QFile(partPath, this);
    if (!m_activeFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(entry.file, "We could not create the download file.");
        delete m_activeFile; m_activeFile = nullptr;
        return;
    }
    m_activeName = entry.file;

    QNetworkRequest req{QUrl(entry.url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_activeReply = m_net->get(req);

    connect(m_activeReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_activeFile) m_activeFile->write(m_activeReply->readAll());
    });
    connect(m_activeReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 rec, qint64 tot) { emit downloadProgress(m_activeName, rec, tot); });
    connect(m_activeReply, &QNetworkReply::finished, this, [this, entry, partPath]() {
        const bool ok = m_activeReply->error() == QNetworkReply::NoError;
        const QString err = m_activeReply->errorString();
        if (m_activeFile) {
            m_activeFile->write(m_activeReply->readAll());
            m_activeFile->close();
            delete m_activeFile; m_activeFile = nullptr;
        }
        m_activeReply->deleteLater();
        m_activeReply = nullptr;

        const QString finalPath = m_modelsDir + "/" + entry.file;
        if (ok) {
            QFile::remove(finalPath);            // replace any prior copy
            if (QFile::rename(partPath, finalPath)) {
                emit downloadFinished(entry.file);
            } else {
                emit downloadFailed(entry.file, "We could not finalize the downloaded file.");
            }
        } else {
            QFile::remove(partPath);             // drop the partial download
            emit downloadFailed(entry.file, err);
        }
    });
}

void ModelManager::cancel() {
    if (m_activeReply) m_activeReply->abort();   // triggers finished() with an error → cleanup
}

} // namespace bthl::spiritbox
