#!/bin/sh
# Makes the Mac disk image: the app, a link to Applications and the read-me in a
# designed Finder window (mac/dmg: background picture, fixed icon positions, no
# toolbar or sidebar), with the app's icon as the volume icon.
#
#   sh mac/make-dmg.sh path/to/wasdmod.app out.dmg ["Read me.txt"]
#
# dmgbuild writes the window layout (.DS_Store) itself, so no Finder window opens
# and it works on a build server. It runs from a virtualenv with the pinned,
# hash-checked packages in mac/dmg/requirements.txt, made once per machine in
# ~/Library/Caches/wasdmod-build (or $WASDMOD_CACHE). The image is put together in
# $TMPDIR, away from iCloud's Desktop sync, and the app's signature is checked
# inside it at the end.
set -e
APP=$1 OUT=$2 README=$3
[ -d "$APP" ] && [ -n "$OUT" ] || { echo "usage: sh mac/make-dmg.sh path/to/App.app out.dmg [read-me]"; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd)
NAME=$(basename "$APP" .app)
VOLNAME=${VOLNAME:-$NAME}

# dmgbuild needs Python 3.10 or newer (macOS's own /usr/bin/python3 is 3.9).
PY=
for p in "$PYTHON" python3 python3.14 python3.13 python3.12 python3.11 python3.10; do
    [ -n "$p" ] && command -v "$p" >/dev/null 2>&1 || continue
    "$p" -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>/dev/null && { PY=$p; break; }
done
[ -n "$PY" ] || { echo "Making the disk image needs Python 3.10 or newer (set PYTHON=/path/to/python3)."; exit 1; }
REQ="$HERE/dmg/requirements.txt"
VENV="${WASDMOD_CACHE:-$HOME/Library/Caches/wasdmod-build}/dmgbuild-$("$PY" -c 'import sys; print("%d.%d" % sys.version_info[:2])')-$(shasum -a 256 "$REQ" | cut -c1-12)"
if ! "$VENV/bin/python" -c 'import dmgbuild' 2>/dev/null; then
    rm -rf "$VENV"
    "$PY" -m venv "$VENV"
    "$VENV/bin/python" -m pip install --quiet --disable-pip-version-check --no-deps --only-binary :all: --require-hashes -r "$REQ"
fi

ICON="$APP/Contents/Resources/$(plutil -extract CFBundleIconFile raw "$APP/Contents/Info.plist")"
case "$ICON" in *.icns) ;; *) ICON="$ICON.icns" ;; esac
[ -f "$ICON" ] || { echo "No app icon at $ICON"; exit 1; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/wasdmod-dmg.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
"$VENV/bin/python" -m dmgbuild -s "$HERE/dmg/settings.py" \
    -D app="$APP" -D readme="$README" -D icon="$ICON" \
    -D background="$HERE/dmg/background.png" -D layout="$HERE/dmg/layout.js" \
    "$VOLNAME" "$WORK/image.dmg" >/dev/null

# The app must still be validly signed inside the image.
MNT="$WORK/mnt"
mkdir "$MNT"
hdiutil attach -quiet -nobrowse -readonly -noautoopen -mountpoint "$MNT" "$WORK/image.dmg"
ok=1
codesign --verify --deep --strict "$MNT/$NAME.app" || ok=0
hdiutil detach -quiet "$MNT" || hdiutil detach -quiet -force "$MNT"
[ $ok = 1 ] || { echo "The app's signature is broken inside the disk image."; exit 1; }

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
mv "$WORK/image.dmg" "$OUT"
