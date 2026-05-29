/**
 * @file src/ui/ModelManagerDialog.h
 * @title ModelManagerDialog - Download / manage Whisper models in-app
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We present the downloadable-model catalog so investigators can pull larger speech
 *          models from bthlcorp.com into their models folder without leaving the app. Only the
 *          base model ships in the installer; everything else is fetched here.
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#ifndef BTHL_SPIRITBOX_MODEL_MANAGER_DIALOG_H
#define BTHL_SPIRITBOX_MODEL_MANAGER_DIALOG_H

#include <QDialog>
#include <QString>
#include "net/ModelManager.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QProgressBar;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace bthl::spiritbox {

class ModelManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelManagerDialog(const QString& modelsDir, QWidget* parent = nullptr);

signals:
    /// We emit after a model finishes downloading so the main window can refresh its picker.
    void modelInstalled(const QString& file);

private:
    void populate(const QList<ModelEntry>& models);
    void startDownloadRow(int row);

    ModelManager m_mgr;
    QList<ModelEntry> m_models;
    QTableWidget* m_table{nullptr};
    QProgressBar* m_progress{nullptr};
    QLabel* m_status{nullptr};
    QPushButton* m_cancelBtn{nullptr};
    int m_downloadingRow{-1};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_MODEL_MANAGER_DIALOG_H
