/**
 * @file src/ui/HelpBrowser.h
 * @title HelpBrowser - In-App Documentation Browser
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We present the project's HTML documentation inside the app via a QWebEngine view
 *          with a navigation sidebar. We compose each page from a shared header + content +
 *          footer at load time (DRY chrome) so every page renders with consistent navigation
 *          and styling, including offline syntax highlighting.
 * @reason Investigators need session templates and how-to guidance at their fingertips during a
 *         live investigation, without leaving the app or going online.
 *
 * CHANGELOG:
 * 2026-05-29 - Initial implementation.
 */

#ifndef BTHL_SPIRITBOX_HELP_BROWSER_H
#define BTHL_SPIRITBOX_HELP_BROWSER_H

#include <QWidget>
#include <QString>
#include <QWebEnginePage>

QT_BEGIN_NAMESPACE
class QListWidget;
class QWebEngineView;
QT_END_NAMESPACE

namespace bthl::spiritbox {

/**
 * @class DocNavPage
 * @purpose We intercept navigation so clicks on local *.html links re-route through the
 *          composer (keeping the shared header/footer chrome), and external links open in the
 *          system browser instead of inside our viewer.
 */
class DocNavPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit DocNavPage(QObject* parent = nullptr);

signals:
    /// We emit the bare page filename (e.g. "faq.html") when a local doc link is clicked.
    void localPageRequested(const QString& fileName);

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
};

/**
 * @class HelpBrowser
 * @purpose We host the navigation sidebar and the web view, and own the header/footer
 *          composition that gives every documentation page consistent chrome.
 */
class HelpBrowser : public QWidget {
    Q_OBJECT

public:
    explicit HelpBrowser(QWidget* parent = nullptr);

    /// We load a documentation page by filename (default "index.html"), composing chrome.
    void showPage(const QString& fileName = QStringLiteral("index.html"));

private:
    /// We locate the docs/ directory across run layouts (env, bundle, source tree, exe-relative).
    static QString resolveDocsDir();

    /// We stitch header.html + <page> + footer.html into a single document.
    QString composePage(const QString& fileName) const;

    QString m_docsDir;
    QString m_header;
    QString m_footer;
    QListWidget* m_nav{nullptr};
    QWebEngineView* m_view{nullptr};
    DocNavPage* m_page{nullptr};
};

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_HELP_BROWSER_H
