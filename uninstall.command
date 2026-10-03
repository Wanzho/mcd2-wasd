#!/bin/sh
# Removes the mod (keeps saved layouts). ./uninstall.command [path/to/bottle]
[ -n "$1" ] && export BOTTLE="$1"
sh "$(dirname "$0")/mac/wasdmod.sh" uninstall && echo "Uninstalled. Restart the game."
