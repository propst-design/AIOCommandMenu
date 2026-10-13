#!/bin/bash
# Builds the stripped Workshop copy: addons/_AIO-release/AIOCommandMenu (same GUID).
# Kept one level down so the game and Workbench never see two copies of the mod in addons/.
# Jacob opens _AIO-release/AIOCommandMenu/addon.gproj in Workbench and publishes from there.
set -e
SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$SRC/../_AIO-release/AIOCommandMenu"
rm -rf "$OUT"; mkdir -p "$OUT"
cp -r "$SRC/Scripts" "$SRC/Configs" "$OUT/"
cp "$SRC/resourceDatabase.rdb" "$SRC/COPYING" "$SRC/LICENSE.md" "$SRC/LICENSE.txt" "$SRC/README.md" "$OUT/"
sed 's/^ TITLE ".*"/ TITLE "AiO Command Menu (Reforger)"/' "$SRC/addon.gproj" > "$OUT/addon.gproj"
cp "$SRC/tools/workshop/"* "$OUT/../" 2>/dev/null || true
echo "Release copy: $OUT"; (cd "$OUT" && find . -type f | sort)
