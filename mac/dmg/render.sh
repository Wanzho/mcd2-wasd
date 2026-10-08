#!/bin/sh
# Renders background.html into background.png (660 x 480) and background@2x.png
# (1320 x 960, marked 144 dpi) with headless Chrome. Run it after changing the page
# and commit the two pictures; the build only uses the pictures.
set -e
cd "$(dirname "$0")"
CHROME=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wasdmod-bg.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
for s in 1 2; do
    # Headless Chrome on macOS writes the screenshot but doesn't always quit
    # afterwards: wait for the file, then stop it.
    "$CHROME" --headless=new --disable-gpu --hide-scrollbars --no-first-run --no-default-browser-check \
        --user-data-dir="$TMP/profile$s" --allow-file-access-from-files \
        --force-device-scale-factor=$s --window-size=660,480 \
        --screenshot="$TMP/bg$s.png" "file://$PWD/background.html" >/dev/null 2>&1 &
    pid=$!
    i=0
    while [ ! -s "$TMP/bg$s.png" ] && [ $i -lt 300 ] && kill -0 $pid 2>/dev/null; do sleep 0.1; i=$((i + 1)); done
    sleep 0.5
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true
    [ -s "$TMP/bg$s.png" ] || { echo "Chrome didn't render background.html"; exit 1; }
done
sips -s dpiWidth 144 -s dpiHeight 144 "$TMP/bg2.png" >/dev/null
for f in "$TMP/bg1.png:660x480" "$TMP/bg2.png:1320x960"; do
    size=$(sips -g pixelWidth -g pixelHeight "${f%%:*}" | awk '/pixel/ { printf "%s%s", sep, $2; sep = "x" }')
    [ "$size" = "${f##*:}" ] || { echo "${f%%:*} is $size, expected ${f##*:}"; exit 1; }
done
cp "$TMP/bg1.png" background.png
cp "$TMP/bg2.png" background@2x.png
echo "Rendered background.png and background@2x.png"
