/**
 * @file src/net/BthlEndpoints.h
 * @title BthlEndpoints - Beyond The Horizon Labs service URLs
 * @author David St John (davestj)
 * @date 2026-05-29
 * @purpose We centralize the bthlcorp.com endpoints used by the in-app updater and model
 *          downloader so there is one place to change the host or paths.
 */

#ifndef BTHL_SPIRITBOX_BTHL_ENDPOINTS_H
#define BTHL_SPIRITBOX_BTHL_ENDPOINTS_H

#include <QString>

namespace bthl::spiritbox {

namespace endpoints {
    /// Base path for all SpiritBox web services.
    inline const QString kBase = QStringLiteral("https://bthlcorp.com/spiritbox");
    /// Returns the latest-version JSON: {version, pkg_url, notes, min_os}.
    inline const QString kUpdateCheck = kBase + QStringLiteral("/updates.php");
    /// Returns the downloadable model catalog (models.json).
    inline const QString kModelCatalog = kBase + QStringLiteral("/models.json");
}

} // namespace bthl::spiritbox

#endif // BTHL_SPIRITBOX_BTHL_ENDPOINTS_H
