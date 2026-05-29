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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
# The effective version is computed by CMake (tag-pinned for releases, else auto-incremented build)
# and written to build/version.txt. We fall back to the CMakeLists baseline before the first build.
read_version() {
    if [ -s "$BUILD/version.txt" ]; then cat "$BUILD/version.txt"
    else grep -m1 -A2 '^project(' "$ROOT/CMakeLists.txt" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1; fi
}
VERSION="$(read_version)"
DIST="$ROOT/dist"
BUILD_APP="$BUILD/BTHL-SpiritBox.app"
# We deploy/sign a STAGING COPY so macdeployqt never embeds Qt frameworks into the dev build
# (which would make build/ load two sets of Qt and crash on a normal dev run).
APP="$DIST/stage/BTHL-SpiritBox.app"
ENTITLEMENTS="$ROOT/resources/entitlements.plist"
PKG="$DIST/BTHL-SpiritBox-${VERSION}.pkg"

SKIP_NOTARIZE=0
[ "${1:-}" = "--skip-notarize" ] && SKIP_NOTARIZE=1

step() { echo ""; echo "━━━ $* ━━━"; }

# ─── 1. Build ─────────────────────────────────────────────────────────────────
step "1/7 Building Release .app"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/homebrew" >/dev/null
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"
[ -d "$BUILD_APP" ] || { echo "ERROR: $BUILD_APP not produced"; exit 1; }
# Stage a clean copy to operate on (keeps build/ dev-runnable).
rm -rf "$DIST/stage"; mkdir -p "$DIST/stage"
cp -R "$BUILD_APP" "$APP"

# ─── 2. macdeployqt ───────────────────────────────────────────────────────────
# We do NOT bundle models inside the .app. Only the base model ships, and it goes in a sibling
# models/ folder (outside the signed bundle) so the in-app downloader can add models later
# without breaking the signature.
step "2/7 Bundling Qt frameworks (macdeployqt)"
MACDEPLOYQT="$(command -v macdeployqt || echo /opt/homebrew/bin/macdeployqt)"
"$MACDEPLOYQT" "$APP" -always-overwrite -no-strip

# ─── 3. Code-sign (hardened runtime, inside-out) ─────────────────────────────
step "3/7 Code-signing with $IDENTITY_APP"
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

# ─── 4. Assemble the per-user payload (app + sibling base model) ──────────────
step "4/7 Assembling install payload (app + base.en in sibling models/)"
PAYLOAD="$DIST/payload"
APPROOT="$PAYLOAD/BTHL-SpiritBox"   # becomes ~/Applications/BTHL/BTHL-SpiritBox
rm -rf "$PAYLOAD"; mkdir -p "$APPROOT/models"
cp -R "$APP" "$APPROOT/BTHL-SpiritBox.app"
if [ ! -s "$ROOT/models/ggml-base.en.bin" ]; then
    echo "base.en missing — fetching models…"; "$ROOT/scripts/fetch-models.sh"
fi
cp -f "$ROOT/models/ggml-base.en.bin" "$APPROOT/models/ggml-base.en.bin"
echo "Payload: BTHL-SpiritBox/{BTHL-SpiritBox.app, models/ggml-base.en.bin}"

# ─── 5. Build the per-user, Installer-signed .pkg ─────────────────────────────
step "5/7 Building per-user Developer ID Installer-signed .pkg"
mkdir -p "$DIST"
COMPONENT="$DIST/BTHL-SpiritBox-component.pkg"
# Install-location /Applications/BTHL is rebased under the user's home by the currentUserHome
# domain in distribution.xml → ~/Applications/BTHL/BTHL-SpiritBox/.
pkgbuild --root "$PAYLOAD" --install-location "/Applications/BTHL" \
    --identifier "$BUNDLE_ID" --version "$VERSION" "$COMPONENT"
productbuild --distribution "$ROOT/resources/distribution.xml" \
    --package-path "$DIST" \
    --sign "$IDENTITY_PKG" --timestamp \
    "$PKG"
rm -f "$COMPONENT"
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
