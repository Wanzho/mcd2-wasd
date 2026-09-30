#!/bin/sh
# Installs the WASD movement mod into a CrossOver Steam bottle.
# Usage: ./install.command [path/to/bottle]   (default: the bottle named "Steam")
set -e
cd "$(dirname "$0")"
BOTTLE="${1:-$HOME/Library/Application Support/CrossOver/Bottles/Steam}"
GAME="$BOTTLE/drive_c/Program Files (x86)/Steam/steamapps/common/Minecraft Dungeons II/Dungeons/Binaries/Win64"
WINE=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine
DLL=build/xinput1_4.dll
[ -f "$DLL" ] || DLL=xinput1_4.dll
[ -f "$DLL" ] || { echo "xinput1_4.dll not found; run ./build.sh first."; exit 1; }
[ -d "$GAME" ] || { echo "Game not found at: $GAME"; echo "Pass your bottle folder as the first argument."; exit 1; }
[ -x "$WINE" ] || { echo "CrossOver not found at /Applications/CrossOver.app"; exit 1; }

if pgrep -f Dungeons-Win64-Shipping >/dev/null; then
    echo "Quit Minecraft Dungeons II first (the game has the mod file open)."; exit 1
fi
if [ -f "$GAME/xinput1_4.dll" ] && ! grep -q -E "WASD mod loaded|Controller mod loaded" "$GAME/xinput1_4.dll"; then
    echo "A different xinput1_4.dll is already in the game folder; not overwriting it."; exit 1
fi
cp "$DLL" "$GAME/xinput1_4.dll"
if [ -f "$GAME/wasd-mod.ini" ] && ! cmp -s wasd-mod.ini "$GAME/wasd-mod.ini"; then
    cp "$GAME/wasd-mod.ini" "$GAME/wasd-mod.ini.bak"
    echo "Previous settings saved as wasd-mod.ini.bak"
fi
cp wasd-mod.ini "$GAME/wasd-mod.ini"
# A personal wasd-mod.txt (from the Keybinder) keeps priority: the mod reads
# whichever of the two files is newer.
if [ -f "$GAME/wasd-mod.txt" ]; then touch "$GAME/wasd-mod.txt"; echo "Your wasd-mod.txt stays in charge (delete it to use the defaults)."; fi

# Wine prefers its own XInput by default; tell it to use the game folder's copy,
# for this game only.
"$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg add \
    'HKCU\Software\Wine\AppDefaults\Dungeons-Win64-Shipping.exe\DllOverrides' \
    /v xinput1_4 /t REG_SZ /d native,builtin /f >/dev/null
# The Mac Option key normally types special characters in CrossOver; send it as
# Alt instead, so holding it shows the cursor (the mod's Cursor key).
# CrossOver can confine the cursor to the wrong window (the mod's small key list)
# instead of the game; clip to the screen area the game asks for instead.
"$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg add \
    'HKCU\Software\Wine\Mac Driver' /v UseConfinementCursorClipping /t REG_SZ /d n /f >/dev/null
for side in Left Right; do
    "$WINE" --bottle "$(basename "$BOTTLE")" --debugmsg -all reg add \
        'HKCU\Software\Wine\Mac Driver' /v ${side}OptionIsAlt /t REG_SZ /d y /f >/dev/null
done
echo "Installed. Restart the game. F9 shows the key list; hold Option for the cursor; the backtick key (\`) switches the mod off/on."
echo "Settings: $GAME/wasd-mod.ini"
