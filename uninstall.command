#!/bin/sh
# Removes the WASD movement mod. Usage: ./uninstall.command [path/to/bottle]
cd "$(dirname "$0")"
BOTTLE="${1:-$HOME/Library/Application Support/CrossOver/Bottles/Steam}"
GAME="$BOTTLE/drive_c/Program Files (x86)/Steam/steamapps/common/Minecraft Dungeons II/Dungeons/Binaries/Win64"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine

if pgrep -f Dungeons-Win64-Shipping >/dev/null; then
    echo "Quit Minecraft Dungeons II first (the game has the mod file open)."; exit 1
fi
if [ -f "$GAME/xinput1_4.dll" ]; then
    if grep -q -E "WASD mod loaded|Controller mod loaded" "$GAME/xinput1_4.dll"; then rm "$GAME/xinput1_4.dll"
    else echo "Leaving $GAME/xinput1_4.dll alone: it is not this mod's file."; fi
fi
rm -f "$GAME/wasd-mod.ini" "$GAME/wasd-mod.log"
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
