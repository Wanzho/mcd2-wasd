#!/bin/sh
# wasdmod for Linux and Steam Deck: starts wasdmod.py, the script beside this file,
# with the system's Python 3. Both are plain text. In a terminal:
#   ./wasdmod-linux.sh            the key layout editor, with the mod's buttons
#   ./wasdmod-linux.sh install    (or status, uninstall, off, on, find; a game folder may follow)
here=$(dirname "$0")
if command -v python3 >/dev/null 2>&1; then exec python3 "$here/wasdmod.py" "$@"; fi
echo "wasdmod needs Python 3 (python3), which this system doesn't seem to have."
exit 1
