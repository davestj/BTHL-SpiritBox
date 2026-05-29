/**
 * @file src/net/ModelManager.h
 * @title ModelManager - In-app Whisper model catalog + downloader
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We fetch the downloadable-model catalog from bthlcorp.com and download model weight
 *          files into the user's models folder, so only the base model ships in the installer
 *          and larger models are pulled on demand.
 *
 * Catalog format (models.json):
 *   { "models": [ { "name": "...", "file": "ggml-small.en.bin",
 *                   "url": "https://bthlcorp.com/spiritbox/downloads/ggml-small.en.bin",
 *                   "size_mb": 466, "description": "...", "bundled": false }, ... ] }
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#ifndef BTHL_SPIRITBOX_MODEL_MANAGER_H
#define BTHL_SPIRITBOX_MODEL_MANAGER_H

#include <QObject>
#include <QString>
#include <QList>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
QT_END_NAMESPACE

namespace bthl::spiritbox {

struct ModelEntry {
    QString name;          ///< Display name ("Small (English)")
    QString file;          ///< Filename ("ggml-small.en.bin")
    QString url;           ///< Absolute download URL
    QString description;
    int sizeMb{0};
    bool bundled{false};   ///< Ships with the installer
    bool installed{false}; ///< Present in the models folder right now
};

class ModelManager : public QObject {
    Q_OBJECT
public:
    explicit ModelManager(QObject* parent = nullptr);

    /// We set the user-writable models directory where downloads are saved and presence checked.
    void setModelsDir(const QString& dir) { m_modelsDir = dir; }
    [[nodiscard]] QString modelsDir() const { return m_modelsDir; }

    /// We fetch and parse models.json, marking each entry installed/not based on the models dir.
    void fetchCatalog();

    /// We download one model to the models dir (atomically via a .part temp + rename).
    void downloadModel(const ModelEntry& entry);

    /// We cancel an in-flight download, if any.
    void cancel();

signals:
    void catalogReady(const QList<ModelEntry>& models);
    void catalogFailed(const QString& reason);
    void downloadProgress(const QString& file, qint64 received, qint64 total);
    void downloadFinished(const QString& file);
    void downloadFailed(const QString& file, const QString& reason);

private:
    bool isInstalled(const QString& file) const;

    QNetworkAccessManager* m_net{nullptr};
    QString m_modelsDir;
    QNetworkReply* m_activeReply{nullptr};
    QFile* m_activeFile{nullptr};
    QString m_activeName;
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_MODEL_MANAGER_H
