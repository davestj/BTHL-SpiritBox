#!/usr/bin/env bash
#
# bump-version.sh — Bump the BTHL-SpiritBox version everywhere from one command.
#
# CMakeLists.txt's project(VERSION) is the single source of truth; the app, Info.plist, and
# installer derive from it. This script also updates the installer's distribution.xml and the
# website (updates.php + product page) so a release is one consistent step.
#
# Cadence (semantic versioning):
#   - PATCH (x.y.Z): bug fixes / hardening, no new features        ./bump-version.sh patch
#   - MINOR (x.Y.0): new backward-compatible features              ./bump-version.sh minor
#   - MAJOR (X.0.0): breaking changes / major releases             ./bump-version.sh major
#   - or set explicitly:                                           ./bump-version.sh 1.2.3
#
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE="$ROOT/CMakeLists.txt"
WEB="$ROOT/../html"   # 00_bthlcorp.com/html

cur="$(grep -m1 -A2 '^project(' "$CMAKE" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
[ -n "$cur" ] || { echo "ERROR: could not read current version from $CMAKE"; exit 1; }
IFS=. read -r MA MI PA <<< "$cur"

arg="${1:-}"
case "$arg" in
    major)         new="$((MA+1)).0.0" ;;
    minor)         new="${MA}.$((MI+1)).0" ;;
    build|patch)   new="${MA}.${MI}.$((PA+1))" ;;   # 3rd component = build number
    [0-9]*.[0-9]*.[0-9]*) new="$arg" ;;
    *) echo "Usage: $0 {major|minor|build|X.Y.Z}  (current: $cur)"; exit 1 ;;
esac

echo "Bumping $cur -> $new"

# 1) CMake project version (single source of truth)
/usr/bin/sed -i '' -E "s/^([[:space:]]*VERSION )$cur/\1$new/" "$CMAKE"

# 2) Installer distribution.xml pkg-ref version
[ -f "$ROOT/resources/distribution.xml" ] && \
    /usr/bin/sed -i '' -E "s/version=\"$cur\"/version=\"$new\"/g" "$ROOT/resources/distribution.xml"

# 3) Website update endpoint + product page (if the website tree is present locally)
[ -f "$WEB/spiritbox/updates.php" ] && \
    /usr/bin/sed -i '' -E "s/(\\\$LATEST[[:space:]]*=[[:space:]]*')$cur(')/\1$new\2/" "$WEB/spiritbox/updates.php"
if [ -f "$WEB/spiritbox.php" ]; then
    /usr/bin/sed -i '' -E "s/(\\\$sb_version[[:space:]]*=[[:space:]]*')$cur(')/\1$new\2/" "$WEB/spiritbox.php"
    /usr/bin/sed -i '' -E "s/(Beta &middot; v)$cur/\1$new/" "$WEB/spiritbox.php"
fi

echo "Updated: CMakeLists.txt, distribution.xml, and website (where present)."

# Commit the app-repo version files and create the release tag. Building from this tag pins the
# version (no build auto-increment). We do NOT push automatically — review, then push.
if [ "${2:-}" != "--no-tag" ] && git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
    git -C "$ROOT" add CMakeLists.txt resources/distribution.xml 2>/dev/null || true
    git -C "$ROOT" commit -m "Release v$new" >/dev/null 2>&1 || true
    if git -C "$ROOT" rev-parse "v$new" >/dev/null 2>&1; then
        echo "Tag v$new already exists — not re-tagging."
    else
        git -C "$ROOT" tag -a "v$new" -m "BTHL-SpiritBox v$new"
        echo "Committed + tagged v$new (not pushed)."
    fi
fi

echo ""
echo "Next:"
echo "  1) Push:      git push && git push origin v$new"
echo "  2) Package:   scripts/package-macos.sh           # build a tag pins the version"
echo "  3) Publish:   upload dist/BTHL-SpiritBox-$new.pkg to spiritbox/downloads/ on the server,"
echo "                then deploy updates.php/models.json/product page via ansible (push.yml)"
