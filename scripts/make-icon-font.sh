#!/usr/bin/env bash
# Generate the icon face: the glyphs the UI needs that LVGL's built-in symbol
# set does not carry and that are too detailed to draw from primitives.
#
#     bash scripts/make-icon-font.sh
#
# Today that is one glyph, the sun on the Display row. It is the same glyph the
# TigerScale uses, which is the point: a drawn approximation of a FontAwesome
# icon is a different icon, however carefully it is drawn.
#
# The font file is fetched at generation time from a PINNED tag and is never
# committed. Pinning is what makes this command produce the same bytes next
# year, which is the whole basis for check-generated.py being able to check it.
#
# Licence: the Font Awesome font files are SIL Open Font License 1.1 and the
# icon artwork is CC BY 4.0. lv_font_conv extracts outlines from the .ttf, so
# it is the OFL that governs what ends up compiled in; both are cited in
# THIRD_PARTY_LICENSES.md.
#
# When the Latin face for accented text is generated, FOLD THIS INTO IT and
# delete this script: lv_font_conv takes several --font in one call, each with
# its own range, so the glyph costs no second face and no extra link in the
# fallback chain. Note that the resulting Opts line then carries two -r, which
# a naive check-generated.py reads as one.
set -euo pipefail
cd "$(dirname "$0")/.."

FA_TAG="6.5.2"
FA_URL="https://github.com/FortAwesome/Font-Awesome/raw/${FA_TAG}/webfonts/fa-solid-900.ttf"
OUT="firmware/src/ui/font_icons_16.c"

command -v npx >/dev/null || { echo "note: npx not on PATH - cannot regenerate here"; exit 3; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Cached, because check-generated.py re-runs this on every verify. Keyed on the
# tag, so bumping FA_TAG fetches afresh rather than silently reusing the old
# outlines. The cache is gitignored: the font file is still never committed.
CACHE=".cache/fonts/fa-solid-900-${FA_TAG}.ttf"
if [ ! -s "$CACHE" ]; then
  mkdir -p "$(dirname "$CACHE")"
  curl -fsSL --max-time 60 -o "$CACHE.part" "$FA_URL" || {
    rm -f "$CACHE.part"
    echo "note: could not fetch Font Awesome ${FA_TAG} - cannot regenerate here"
    exit 3
  }
  mv "$CACHE.part" "$CACHE"
fi
cp "$CACHE" "$TMP/fa.ttf"

# --size 16 matches the 22 px icon box; --bpp 4 because a 1-bit sun at this
# size is a staircase. -o is a temporary name so the Opts line recorded in the
# output does not carry a machine-specific path.
( cd "$TMP" && npx --yes lv_font_conv \
    --font fa.ttf -r 0xF185 \
    --size 16 --bpp 4 --format lvgl --no-compress \
    --lv-include lvgl.h -o font_icons_16.c ) >/dev/null

cp "$TMP/font_icons_16.c" "$OUT"
echo "OK -> $OUT   (Font Awesome ${FA_TAG}, U+F185, $(wc -c < "$OUT") bytes of source)"
