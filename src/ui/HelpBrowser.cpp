/**
 * @file src/ui/HelpBrowser.cpp
 * @title HelpBrowser - In-App Documentation Browser (implementation)
 * @author David St John (davestj)
 * @date 2026-05-29
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#include "ui/HelpBrowser.h"

#include <QListWidget>
#include <QHBoxLayout>
#include <QWebEngineView>
#include <QWebEngineSettings>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QUrl>
#include <QDebug>
#include <array>

namespace bthl::spiritbox {

namespace {

/// We list the documentation pages and their sidebar labels in display order.
struct DocEntry { const char* file; const char* title; };
constexpr std::array<DocEntry, 11> kPages = {{
    {"index.html",                 "Home"},
    {"getting-started.html",       "Getting Started"},
    {"investigator-guide.html",    "Investigator Guide"},
    {"evp-session-templates.html", "Session Templates"},
    {"capture-modes.html",         "Capture Modes"},
    {"hardware-setup.html",        "Hardware Setup"},
    {"whisper-models.html",        "Whisper Models"},
    {"sweep-profiles.html",        "Sweep Profiles"},
    {"troubleshooting.html",       "Troubleshooting"},
    {"faq.html",                   "FAQ"},
    {"about.html",                 "About"},
}};

QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

} // namespace

// ─── DocNavPage ─────────────────────────────────────────────────────────────────

DocNavPage::DocNavPage(QObject* parent) : QWebEnginePage(parent) {}

bool DocNavPage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool /*isMainFrame*/) {
    // We only intervene on actual user link clicks; the initial setHtml load and JS-driven
    // navigation pass through untouched.
    if (type == NavigationType::NavigationTypeLinkClicked) {
        if (url.scheme().startsWith("http")) {
            QDesktopServices::openUrl(url);   // External links open in the system browser
            return false;
        }
        const QString fileName = url.fileName();
        if (fileName.endsWith(".html", Qt::CaseInsensitive)) {
            emit localPageRequested(fileName);  // Re-route through the DRY composer
            return false;
        }
    }
    return true;
}

// ─── HelpBrowser ────────────────────────────────────────────────────────────────

HelpBrowser::HelpBrowser(QWidget* parent) : QWidget(parent) {
    setWindowTitle("BTHL-SpiritBox — Documentation");
    setWindowFlag(Qt::Window, true);
    resize(1100, 760);

    m_docsDir = resolveDocsDir();
    m_header = readFile(m_docsDir + "/header.html");
    m_footer = readFile(m_docsDir + "/footer.html");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setFixedWidth(210);
    m_nav->setStyleSheet(
        "QListWidget { background:#0b1024; color:#9aa6c8; border:0; border-right:1px solid #25305a;"
        " font-size:14px; outline:0; }"
        "QListWidget::item { padding:9px 14px; }"
        "QListWidget::item:selected { background:#18203f; color:#ffd700; }"
        "QListWidget::item:hover { color:#36e0e0; }");
    for (const auto& p : kPages) {
        auto* item = new QListWidgetItem(QString::fromLatin1(p.title), m_nav);
        item->setData(Qt::UserRole, QString::fromLatin1(p.file));
    }
    layout->addWidget(m_nav);

    m_view = new QWebEngineView(this);
    m_page = new DocNavPage(m_view);
    m_view->setPage(m_page);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    layout->addWidget(m_view, 1);

    // We sync the sidebar selection to a clicked nav item and to in-page link clicks.
    connect(m_nav, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        showPage(item->data(Qt::UserRole).toString());
    });
    // We MUST defer the re-compose: localPageRequested is emitted from inside QtWebEngine's
    // acceptNavigationRequest (a navigation throttle). Calling setHtml() synchronously there
    // starts a nested navigation and aborts the engine. A queued connection runs showPage()
    // only after acceptNavigationRequest has returned, breaking the reentrancy.
    connect(m_page, &DocNavPage::localPageRequested, this, &HelpBrowser::showPage,
            Qt::QueuedConnection);

    showPage();
}

QString HelpBrowser::resolveDocsDir() {
    const QByteArray override = qgetenv("BTHL_SPIRITBOX_DOCS");
    if (!override.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(override))) {
        return QString::fromLocal8Bit(override);
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/../Resources/docs",   // macOS .app bundle
        appDir + "/../docs",             // running from build/
        appDir + "/docs",
        QStringLiteral(BTHL_SPIRITBOX_SOURCE_DIR) + "/docs",
    };
    for (const QString& path : candidates) {
        const QString canonical = QDir(path).canonicalPath();
        if (!canonical.isEmpty() && QFileInfo(canonical).isDir() &&
            QFileInfo::exists(canonical + "/header.html")) {
            return canonical;
        }
    }
    return QStringLiteral(BTHL_SPIRITBOX_SOURCE_DIR) + "/docs";
}

QString HelpBrowser::composePage(const QString& fileName) const {
    const QString content = readFile(m_docsDir + "/" + fileName);
    if (content.isEmpty()) {
        return m_header +
               "<h1>Page not found</h1><p>We could not load <code>" + fileName.toHtmlEscaped() +
               "</code> from the documentation folder:</p><pre><code>" +
               m_docsDir.toHtmlEscaped() + "</code></pre>" +
               m_footer;
    }
    return m_header + content + m_footer;
}

void HelpBrowser::showPage(const QString& fileName) {
    const QString html = composePage(fileName);
    // We give the content a file:// base URL so relative assets (CSS/JS/img) and links resolve.
    m_view->setHtml(html, QUrl::fromLocalFile(m_docsDir + "/"));

    // We keep the sidebar selection in step with whatever page is showing.
    for (int i = 0; i < m_nav->count(); ++i) {
        if (m_nav->item(i)->data(Qt::UserRole).toString() == fileName) {
            m_nav->setCurrentRow(i);
            break;
        }
    }
}

} // namespace bthl::spiritbox
