#!/usr/bin/env bash
#
# generate-assets.sh — We rasterize the BTHL-SpiritBox vector branding into every
# platform asset: macOS .icns, Windows .ico, Linux hicolor PNGs + .desktop, the
# embedded window icon, and the loading splash.
#
# Requires: rsvg-convert, ImageMagick (magick or convert), and on macOS iconutil.
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
ICON_SVG="$HERE/icon.svg"
SPLASH_SVG="$HERE/splash.svg"

ICONS_DIR="$ROOT/assets/icons"
LINUX_DIR="$ROOT/assets/linux"
mkdir -p "$ICONS_DIR" "$LINUX_DIR"

# We pick whichever ImageMagick entrypoint exists.
if command -v magick >/dev/null 2>&1; then IM="magick"; else IM="convert"; fi

png() { # png <size> <outfile>
    rsvg-convert -w "$1" -h "$1" "$ICON_SVG" -o "$2"
}

echo "==> Base PNGs"
for s in 16 32 64 128 256 512 1024; do
    png "$s" "$ICONS_DIR/icon-${s}.png"
done

echo "==> macOS .icns"
ICONSET="$(mktemp -d)/AppIcon.iconset"
mkdir -p "$ICONSET"
png 16   "$ICONSET/icon_16x16.png"
png 32   "$ICONSET/icon_16x16@2x.png"
png 32   "$ICONSET/icon_32x32.png"
png 64   "$ICONSET/icon_32x32@2x.png"
png 128  "$ICONSET/icon_128x128.png"
png 256  "$ICONSET/icon_128x128@2x.png"
png 256  "$ICONSET/icon_256x256.png"
png 512  "$ICONSET/icon_256x256@2x.png"
png 512  "$ICONSET/icon_512x512.png"
png 1024 "$ICONSET/icon_512x512@2x.png"
if command -v iconutil >/dev/null 2>&1; then
    iconutil -c icns "$ICONSET" -o "$ICONS_DIR/AppIcon.icns"
else
    echo "    (iconutil not present — skipping .icns; macOS only)"
fi

echo "==> Windows .ico"
$IM "$ICONS_DIR/icon-16.png" "$ICONS_DIR/icon-32.png" "$ICONS_DIR/icon-64.png" \
    "$ICONS_DIR/icon-128.png" "$ICONS_DIR/icon-256.png" "$ICONS_DIR/app.ico"

echo "==> Linux hicolor set"
for s in 16 32 64 128 256 512; do
    cp "$ICONS_DIR/icon-${s}.png" "$LINUX_DIR/bthl-spiritbox-${s}.png"
done

echo "==> Splash"
rsvg-convert -w 900 -h 560 "$SPLASH_SVG" -o "$ROOT/assets/branding/splash.png"

echo "==> Done. Assets written to assets/icons, assets/linux, assets/branding."
