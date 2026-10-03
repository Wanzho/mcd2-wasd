#!/bin/sh
# Installs the mod from this source folder (after ./build.sh) into the CrossOver
# bottle that has the game. Players use the wasdmod app (dist/wasdmod-Mac.dmg).
# ./install.command [path/to/bottle]
[ -n "$1" ] && export BOTTLE="$1"
sh "$(dirname "$0")/mac/wasdmod.sh" install && echo "Installed. Restart the game."
