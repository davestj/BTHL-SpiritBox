/**
 * @file src/ui/ModelManagerDialog.cpp
 * @author David St John (davestj)
 * @date 2026-05-29
 */

#include "ui/ModelManagerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

namespace bthl::spiritbox {

ModelManagerDialog::ModelManagerDialog(const QString& modelsDir, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Download Speech Models");
    resize(680, 420);
    m_mgr.setModelsDir(modelsDir);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(
        QString("Models install to:\n%1\n\nThe base model ships with the app; download larger, "
                "more accurate models on demand.").arg(modelsDir), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Model", "Size", "Status", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    auto* row = new QHBoxLayout();
    m_status = new QLabel("Loading model catalog…", this);
    m_cancelBtn = new QPushButton("Cancel Download", this);
    m_cancelBtn->setVisible(false);
    row->addWidget(m_status, 1);
    row->addWidget(m_cancelBtn);
    layout->addLayout(row);

    connect(m_cancelBtn, &QPushButton::clicked, &m_mgr, &ModelManager::cancel);

    connect(&m_mgr, &ModelManager::catalogReady, this, [this](const QList<ModelEntry>& m) {
        m_models = m;
        populate(m);
        m_status->setText(QString("%1 models available.").arg(m.size()));
    });
    connect(&m_mgr, &ModelManager::catalogFailed, this, [this](const QString& why) {
        m_status->setText("Could not load catalog: " + why);
    });
    connect(&m_mgr, &ModelManager::downloadProgress, this,
            [this](const QString&, qint64 rec, qint64 tot) {
                if (tot > 0) { m_progress->setMaximum(100);
                               m_progress->setValue(static_cast<int>(rec * 100 / tot)); }
            });
    connect(&m_mgr, &ModelManager::downloadFinished, this, [this](const QString& file) {
        m_progress->setVisible(false);
        m_cancelBtn->setVisible(false);
        m_status->setText("Installed " + file);
        m_downloadingRow = -1;
        for (auto& e : m_models) if (e.file == file) e.installed = true;
        populate(m_models);
        emit modelInstalled(file);
    });
    connect(&m_mgr, &ModelManager::downloadFailed, this, [this](const QString& file, const QString& why) {
        m_progress->setVisible(false);
        m_cancelBtn->setVisible(false);
        m_downloadingRow = -1;
        m_status->setText("Download failed: " + file);
        QMessageBox::warning(this, "Download Failed", file + "\n\n" + why);
        populate(m_models);
    });

    m_mgr.fetchCatalog();
}

void ModelManagerDialog::populate(const QList<ModelEntry>& models) {
    m_table->setRowCount(models.size());
    for (int i = 0; i < models.size(); ++i) {
        const ModelEntry& e = models[i];
        m_table->setItem(i, 0, new QTableWidgetItem(e.name.isEmpty() ? e.file : e.name));
        m_table->setItem(i, 1, new QTableWidgetItem(e.sizeMb > 0 ? QString("%1 MB").arg(e.sizeMb) : "—"));
        m_table->setItem(i, 2, new QTableWidgetItem(
            e.installed ? "Installed" : (e.bundled ? "Bundled" : "Available")));
        if (e.installed) {
            m_table->setCellWidget(i, 3, nullptr);
        } else {
            auto* btn = new QPushButton("Download", m_table);
            btn->setEnabled(m_downloadingRow < 0);
            connect(btn, &QPushButton::clicked, this, [this, i]() { startDownloadRow(i); });
            m_table->setCellWidget(i, 3, btn);
        }
    }
}

void ModelManagerDialog::startDownloadRow(int row) {
    if (row < 0 || row >= m_models.size()) return;
    m_downloadingRow = row;
    m_progress->setVisible(true);
    m_progress->setMaximum(0);  // busy until first progress tick
    m_cancelBtn->setVisible(true);
    m_status->setText("Downloading " + m_models[row].file + "…");
    populate(m_models);  // disable other Download buttons
    m_mgr.downloadModel(m_models[row]);
}

} // namespace bthl::spiritbox
