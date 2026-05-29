#!/usr/bin/env bash
#
# package-macos.sh — Build, bundle, code-sign, package, notarize, and staple BTHL-SpiritBox
# into a distributable, Gatekeeper-approved macOS .pkg installer.
#
# Pipeline:
#   1. Configure + build a Release .app (CMake)
#   2. Bundle the Whisper models into Contents/Resources/models
#   3. macdeployqt — copy Qt frameworks/plugins (incl. QtWebEngine) into the bundle and fix rpaths
#   4. Code-sign inside-out with Developer ID Application + hardened runtime + entitlements
#   5. Build a Developer ID Installer-signed .pkg (productbuild)
#   6. Notarize via notarytool (Keychain profile) and staple the ticket
#   7. Verify with codesign/spctl/stapler
#
# Prerequisites:
#   - Xcode command line tools; Homebrew Qt (provides macdeployqt)
#   - Signing identities in the login keychain:
#       "Developer ID Application: David St John (FCA38UPLY3)"
#       "Developer ID Installer:   David St John (FCA38UPLY3)"
#   - A notarytool Keychain profile (created once):
#       xcrun notarytool store-credentials bthl-notary \
#            --apple-id <you@email> --team-id FCA38UPLY3 --password <app-specific-password>
#
# Usage:
#   scripts/package-macos.sh                 # full pipeline incl. notarize + staple
#   scripts/package-macos.sh --skip-notarize # sign + package only (no notarize/staple)
#
set -euo pipefail

# ─── Config (override via env) ───────────────────────────────────────────────
TEAM_ID="${TEAM_ID:-FCA38UPLY3}"
IDENTITY_APP="${IDENTITY_APP:-Developer ID Application: David St John (${TEAM_ID})}"
IDENTITY_PKG="${IDENTITY_PKG:-Developer ID Installer: David St John (${TEAM_ID})}"
NOTARY_PROFILE="${NOTARY_PROFILE:-bthl-notary}"
BUNDLE_ID="com.beyondthehorizonlabs.spiritbox"
VERSION="1.0.0"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
APP="$BUILD/BTHL-SpiritBox.app"
ENTITLEMENTS="$ROOT/resources/entitlements.plist"
PKG="$DIST/BTHL-SpiritBox-${VERSION}.pkg"

SKIP_NOTARIZE=0
[ "${1:-}" = "--skip-notarize" ] && SKIP_NOTARIZE=1

step() { echo ""; echo "━━━ $* ━━━"; }

# ─── 1. Build ─────────────────────────────────────────────────────────────────
step "1/7 Building Release .app"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew" >/dev/null
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"
[ -d "$APP" ] || { echo "ERROR: $APP not produced"; exit 1; }

# ─── 2. Bundle models ─────────────────────────────────────────────────────────
step "2/7 Bundling Whisper models into the app"
if ! ls "$ROOT"/models/*.bin >/dev/null 2>&1; then
    echo "No models in models/ — fetching them first."
    "$ROOT/scripts/fetch-models.sh"
fi
mkdir -p "$APP/Contents/Resources/models"
cp -f "$ROOT"/models/*.bin "$APP/Contents/Resources/models/"
echo "Bundled: $(ls "$APP/Contents/Resources/models" | tr '\n' ' ')"

# ─── 3. macdeployqt ─────────────────────────────────────────────────────────--
step "3/7 Bundling Qt frameworks (macdeployqt)"
MACDEPLOYQT="$(command -v macdeployqt || echo /opt/homebrew/bin/macdeployqt)"
"$MACDEPLOYQT" "$APP" -always-overwrite -no-strip

# ─── 4. Code-sign (hardened runtime, inside-out) ─────────────────────────────
step "4/7 Code-signing with $IDENTITY_APP"
SIGN=(codesign --force --options runtime --timestamp --sign "$IDENTITY_APP")
# ORDER MATTERS: sign deepest-nested code FIRST so an inner signature never invalidates the
# seal of a bundle that contains it. The QtWebEngineProcess.app helper lives *inside*
# QtWebEngineCore.framework, so it must be signed before that framework is sealed.
# 1) WebEngine helper app(s) — need the entitlements (JIT) + hardened runtime
find "$APP" -name "QtWebEngineProcess.app" -print0 2>/dev/null \
    | while IFS= read -r -d '' helper; do
        "${SIGN[@]}" --entitlements "$ENTITLEMENTS" "$helper"
      done
# 2) Standalone dylibs / .so plugins
find "$APP/Contents" \( -name "*.dylib" -o -name "*.so" \) -print0 2>/dev/null \
    | while IFS= read -r -d '' f; do "${SIGN[@]}" "$f" 2>/dev/null || true; done
# 3) Frameworks (now seal over the already-signed helper)
find "$APP/Contents/Frameworks" -type d -name "*.framework" -print0 2>/dev/null \
    | while IFS= read -r -d '' fw; do "${SIGN[@]}" "$fw" 2>/dev/null || true; done
# 4) Finally the main app with entitlements
codesign --force --options runtime --timestamp \
    --entitlements "$ENTITLEMENTS" --sign "$IDENTITY_APP" "$APP"
echo "Verifying signature…"
codesign --verify --deep --strict --verbose=2 "$APP"

# ─── 5. Build signed .pkg ─────────────────────────────────────────────────────
step "5/7 Building Developer ID Installer-signed .pkg"
mkdir -p "$DIST"
productbuild --component "$APP" /Applications \
    --sign "$IDENTITY_PKG" --timestamp \
    --identifier "$BUNDLE_ID" --version "$VERSION" \
    "$PKG"
echo "Built: $PKG"

# ─── 6. Notarize + staple ─────────────────────────────────────────────────────
if [ "$SKIP_NOTARIZE" -eq 1 ]; then
    step "6/7 Notarization SKIPPED (--skip-notarize)"
else
    step "6/7 Notarizing via Keychain profile '$NOTARY_PROFILE'"
    if ! xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1; then
        echo "ERROR: notarytool profile '$NOTARY_PROFILE' not found. Create it once with:"
        echo "  xcrun notarytool store-credentials $NOTARY_PROFILE \\"
        echo "       --apple-id <you@email> --team-id $TEAM_ID --password <app-specific-password>"
        echo "Then re-run, or use --skip-notarize. The signed .pkg is at: $PKG"
        exit 2
    fi
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    step "Stapling notarization ticket"
    xcrun stapler staple "$PKG"
fi

# ─── 7. Verify ────────────────────────────────────────────────────────────────
step "7/7 Verification"
spctl --assess --type install --verbose=4 "$PKG" 2>&1 || true
xcrun stapler validate "$PKG" 2>&1 || true
echo ""
echo "✅ Done: $PKG"
