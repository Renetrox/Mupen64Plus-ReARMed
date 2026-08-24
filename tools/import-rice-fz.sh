#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/mupen64plus-video-rice-fz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

FZ_REPO="https://github.com/fzurita/mupen64plus-ae.git"
FZ_BRANCH="fz-master"

printf 'Importing Rice from Mupen64Plus FZ (%s)...\n' "$FZ_BRANCH"
git clone --depth 1 --branch "$FZ_BRANCH" "$FZ_REPO" "$TMP/mupen64plus-ae"

rm -rf "$DEST/upstream"
mkdir -p "$DEST"
cp -a "$TMP/mupen64plus-ae/mupen64plus-video-rice/upstream" "$DEST/upstream"
cp -a "$TMP/mupen64plus-ae/app/src/main/assets/mupen64plus_data/RiceVideoLinux.ini" "$DEST/RiceVideoLinux.ini"

cat > "$DEST/FZ_SOURCE_REVISION.txt" <<EOF
Repository: $FZ_REPO
Branch: $FZ_BRANCH
Commit: $(git -C "$TMP/mupen64plus-ae" rev-parse HEAD)
Imported: $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF

printf 'RiceFZ source imported into %s\n' "$DEST"
printf 'FZ commit: %s\n' "$(git -C "$TMP/mupen64plus-ae" rev-parse HEAD)"
