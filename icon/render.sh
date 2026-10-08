#!/bin/sh
# Renders the app icons from the two SVGs (needs Google Chrome; run by hand after
# changing an SVG, and commit the PNGs: build.sh only reads them).
#   icon-mac.svg   on its rounded tile, for the Mac app and the website
#   icon-flat.svg  without the tile, for the Windows setup
set -e
cd "$(dirname "$0")"
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
T=$(mktemp -d)
for n in mac flat; do
    printf '<!doctype html><html style="background:transparent"><body style="margin:0;background:transparent"><img src="%s" width="1024" height="1024">' "$PWD/icon-$n.svg" > "$T/$n.html"
    "$CHROME" --headless=new --disable-gpu --hide-scrollbars --default-background-color=00000000 --window-size=1024,1024 --screenshot="$T/$n.png" "file://$T/$n.html" 2>/dev/null
done
mkdir -p png
for s in 16 24 32 48 64 128 256 512 1024; do
    sips -z $s $s "$T/mac.png" --out "png/mac-$s.png" >/dev/null
    sips -z $s $s "$T/flat.png" --out "png/flat-$s.png" >/dev/null
done
rm -rf "$T"
