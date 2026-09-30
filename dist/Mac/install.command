#!/bin/sh
# Installs the Minecraft Dungeons II controller mod into a CrossOver bottle.
# Double-click it, or: ./install.command [path/to/bottle]
set -e
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
DLL=build/xinput1_4.dll
[ -f "$DLL" ] || DLL=xinput1_4.dll
[ -f "$DLL" ] || { echo "xinput1_4.dll not found next to this script."; exit 1; }
[ -d "$GAME" ] || { echo "Minecraft Dungeons II wasn't found in a CrossOver bottle."; echo "Run: ./install.command \"path/to/your bottle\""; exit 1; }
[ -x "$WINE" ] || { echo "CrossOver not found at /Applications/CrossOver.app"; exit 1; }

if pgrep -f Dungeons-Win64-Shipping >/dev/null; then
    echo "Quit Minecraft Dungeons II first (the game has the mod file open)."; exit 1
fi
if [ -f "$GAME/xinput1_4.dll" ] && ! grep -q -E "WASD mod loaded|Controller mod loaded" "$GAME/xinput1_4.dll"; then
    echo "A different xinput1_4.dll is already in the game folder; not overwriting it."; exit 1
fi
cp "$DLL" "$GAME/xinput1_4.dll"
# Older versions named the files wasd-mod... and wasdmod.ini; saved layouts keep
# their date, edited settings are kept as default.txt.bak.
for f in "$GAME"/wasd-mod*.txt; do [ -e "$f" ] && mv -f "$f" "$GAME/wasdmod${f##*/wasd-mod}"; done
for old in wasd-mod.ini wasdmod.ini; do
    [ -f "$GAME/$old" ] || continue
    cmp -s default.txt "$GAME/$old" || cp "$GAME/$old" "$GAME/default.txt.bak"
    rm -f "$GAME/$old"
done
rm -f "$GAME/wasd-mod.log"
if [ -f "$GAME/default.txt" ] && ! cmp -s default.txt "$GAME/default.txt"; then
    cp "$GAME/default.txt" "$GAME/default.txt.bak"
    echo "Previous settings saved as default.txt.bak"
fi
cp default.txt "$GAME/default.txt"
# A saved layout (author.txt or wasdmod-MMDDYY.txt from the Keybinder) keeps
# priority: the mod reads whichever settings file is newest.
NEWEST=$(ls -t "$GAME"/author.txt "$GAME"/wasdmod*.txt 2>/dev/null | head -1)
if [ -n "$NEWEST" ]; then touch "$NEWEST"; echo "Your saved layout $(basename "$NEWEST") stays in charge (delete it to use the defaults)."; fi

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
echo "Settings: $GAME/default.txt (your own layouts: author.txt, wasdmod-MMDDYY.txt)"
