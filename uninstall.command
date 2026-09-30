#!/bin/sh
# Removes the Minecraft Dungeons II controller mod. Double-click it, or:
# ./uninstall.command [path/to/bottle]
cd "$(dirname "$0")"
# Finds the game: the bottle given as an argument, or any CrossOver bottle that has
# Minecraft Dungeons II installed through Steam.
SUB="drive_c/Program Files (x86)/Steam/steamapps/common/Minecraft Dungeons II/Dungeons/Binaries/Win64"
if [ -n "$1" ]; then BOTTLE="$1"
else
    BOTTLE="$HOME/Library/Application Support/CrossOver/Bottles/Steam"
    [ -d "$BOTTLE/$SUB" ] || for b in "$HOME/Library/Application Support/CrossOver/Bottles"/*/; do
        [ -d "$b$SUB" ] && { BOTTLE="${b%/}"; break; }
    done
fi
GAME="$BOTTLE/$SUB"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine

if pgrep -f Dungeons-Win64-Shipping >/dev/null; then
    echo "Quit Minecraft Dungeons II first (the game has the mod file open)."; exit 1
fi
if [ -f "$GAME/xinput1_4.dll" ]; then
    if grep -q -E "WASD mod loaded|Controller mod loaded" "$GAME/xinput1_4.dll"; then rm "$GAME/xinput1_4.dll"
    else echo "Leaving $GAME/xinput1_4.dll alone: it is not this mod's file."; fi
fi
rm -f "$GAME/default.txt" "$GAME/default.txt.bak" "$GAME/wasdmod.log" "$GAME/wasdmod.ini" "$GAME/wasd-mod.ini" "$GAME/wasd-mod.log"
"$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg delete \
    'HKCU\Software\Wine\AppDefaults\Dungeons-Win64-Shipping.exe\DllOverrides' \
    /v xinput1_4 /f >/dev/null 2>&1
"$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg delete \
    'HKCU\Software\Wine\Mac Driver' /v UseConfinementCursorClipping /f >/dev/null 2>&1
for side in Left Right; do
    "$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg delete \
        'HKCU\Software\Wine\Mac Driver' /v ${side}OptionIsAlt /f >/dev/null 2>&1
done
echo "Uninstalled. Restart the game to go back to click-to-move only."
