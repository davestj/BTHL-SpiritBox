#!/usr/bin/env bash
#
# fetch-models.sh — Download the Whisper speech models bundled in the BTHL-SpiritBox installer.
#
# Whisper models are pre-trained GGML weight files (not "compiled" — downloaded). The whisper.cpp
# *library* itself is compiled as part of the normal CMake build (lib/whisper.cpp). This script
# fetches the model weights into models/ so the packaging step can bundle them into the .pkg.
#
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODELS="$HERE/models"
mkdir -p "$MODELS"

BASE="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
FILES=(ggml-base.en.bin ggml-small.en.bin ggml-medium.en.bin ggml-large-v3-turbo.bin)

for f in "${FILES[@]}"; do
    if [ -s "$MODELS/$f" ]; then
        echo "==> $f already present, skipping"
        continue
    fi
    echo "==> downloading $f"
    curl -L --fail --progress-bar -o "$MODELS/$f" "$BASE/$f"
done

echo "==> verifying GGML magic (should be 'ggml')"
for f in "${FILES[@]}"; do
    magic=$(head -c4 "$MODELS/$f" | tr -d '\0')
    printf "    %-28s %s\n" "$f" "${magic:-?}"
done
echo "==> models ready in $MODELS"
