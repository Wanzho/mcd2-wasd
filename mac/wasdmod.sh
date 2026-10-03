#!/bin/sh
# The Mac installer's work, run by wasdmod.app (and install.command /
# uninstall.command). Installs the Minecraft Dungeons II controller mod into the
# CrossOver bottle that has the game.
#
#   wasdmod.sh status             game=, state=none|current|older|other|off, layout=, own=
#   wasdmod.sh install            FORCE=1 replaces another mod's xinput1_4.dll (kept as .other)
#   wasdmod.sh uninstall          ALL=1 also deletes the saved layouts
#   wasdmod.sh off | on           the game starts without the mod (xinput1_4.dll.off) / with it again
#   wasdmod.sh record start|stop  Record Logs: the mod also logs every key it handles
#   wasdmod.sh report [FILE]      one text file for a bug report (default: on the Desktop)
#   wasdmod.sh use default|recommended|own
#   wasdmod.sh load FILE          a layout file the key layout editor downloaded
#   wasdmod.sh editor | folder    opens the key layout editor / the game folder
#
# BOTTLE=/path/to/bottle picks the bottle; otherwise it's the one with the game
# installed through Steam.
HERE=$(cd "$(dirname "$0")" && pwd)
if [ -f "$HERE/xinput1_4.dll" ]; then # inside wasdmod.app
    DLL="$HERE/xinput1_4.dll"; DEF="$HERE/default.txt"; REC="$HERE/author.txt"; EDITOR="$HERE/Key Layout Editor.html"
else # the source folder, after ./build.sh
    R="$HERE/.."; DLL="$R/build/xinput1_4.dll"; DEF="$R/default.txt"; REC="$R/author.txt"; EDITOR="$R/Keybinder.html"
fi
# The game's Win64 folder in a bottle: installed through Steam, or (if the Xbox
# services ever run in CrossOver) the Minecraft Launcher / Xbox app's XboxGames.
gameIn() {
    for sub in "drive_c/Program Files (x86)/Steam/steamapps/common/Minecraft Dungeons II/Dungeons/Binaries/Win64" \
               "drive_c/XboxGames/Minecraft Dungeons II/Content/Dungeons/Binaries/Win64"; do
        [ -d "$1/$sub" ] && { echo "$1/$sub"; return 0; }
    done
    return 1
}
BOTTLES="$HOME/Library/Application Support/CrossOver/Bottles"
if [ -n "$BOTTLE" ]; then
    BOTTLE="${BOTTLE%/}"; GAME=$(gameIn "$BOTTLE")
elif GAME=$(gameIn "$BOTTLES/Steam"); then
    BOTTLE="$BOTTLES/Steam"
else
    for b in "$BOTTLES"/*/; do GAME=$(gameIn "${b%/}") && { BOTTLE="${b%/}"; break; }; done
fi

fail() { echo "$*" >&2; exit 1; }
ours() { grep -q -E "WASD mod loaded|Controller mod loaded" "$1"; }
# The [p] keeps pgrep from matching a shell whose command line has the name in it.
running() { pgrep -f 'Dungeons-Win64-Shi[p]ping' >/dev/null; }
needGame() { [ -d "$GAME" ] || fail "Minecraft Dungeons II wasn't found in a CrossOver bottle."; }
needQuit() { running && fail "Minecraft Dungeons II is running. Quit it, then try again."; return 0; }
# The mod reads whichever settings file is newest.
active() { (cd "$GAME" && ls -t default.txt author.txt wasdmod*.txt wasdmod.ini 2>/dev/null | head -1); }
own() { (cd "$GAME" && ls -t wasdmod*.txt 2>/dev/null | head -1); }
wine() {
    for w in /Applications/CrossOver.app "$HOME/Applications/CrossOver.app"; do
        w="$w/Contents/SharedSupport/CrossOver/bin/wine"
        [ -x "$w" ] && { "$w" --bottle "$(basename "$BOTTLE")" --debugmsg -all "$@"; return; }
    done
    return 1
}
# Replaces a file; a different (edited) one is kept as NAME.bak.
put() { [ "$1" -ef "$GAME/$2" ] && { touch "$1"; return; }; [ -f "$GAME/$2" ] && ! cmp -s "$1" "$GAME/$2" && cp "$GAME/$2" "$GAME/$2.bak"; cp "$1" "$GAME/$2"; }

case "$1" in
status)
    [ -d "$GAME" ] || { echo "game="; exit 0; }
    echo "game=$GAME"
    if [ ! -f "$GAME/xinput1_4.dll" ]; then
        if [ -f "$GAME/xinput1_4.dll.off" ] && ours "$GAME/xinput1_4.dll.off"; then echo "state=off"; else echo "state=none"; fi
    elif ! ours "$GAME/xinput1_4.dll"; then echo "state=other"
    elif cmp -s "$DLL" "$GAME/xinput1_4.dll"; then echo "state=current"
    else echo "state=older"; fi
    echo "layout=$(active)"
    echo "own=$(own)"
    [ -f "$GAME/wasdmod-record.flag" ] && echo "recording=yes" || echo "recording=no"
    ;;
install)
    needGame; needQuit
    wine reg query 'HKCU\Software\Wine' >/dev/null 2>&1 || fail "CrossOver wasn't found in Applications."
    if [ -f "$GAME/xinput1_4.dll" ] && ! ours "$GAME/xinput1_4.dll"; then
        [ "$FORCE" = 1 ] || fail "The game folder already has a different xinput1_4.dll (another mod?)."
        cp "$GAME/xinput1_4.dll" "$GAME/xinput1_4.dll.other"
    fi
    cp "$DLL" "$GAME/xinput1_4.dll" || fail "Couldn't write to the game folder."
    [ -f "$GAME/xinput1_4.dll.off" ] && ours "$GAME/xinput1_4.dll.off" && rm -f "$GAME/xinput1_4.dll.off" # installing turns it back on
    # Older versions named the files wasd-mod... and wasdmod.ini; saved layouts
    # keep their date, edited settings are kept as default.txt.bak.
    for f in "$GAME"/wasd-mod*.txt; do [ -e "$f" ] && mv -f "$f" "$GAME/wasdmod${f##*/wasd-mod}"; done
    for old in wasd-mod.ini wasdmod.ini; do
        [ -f "$GAME/$old" ] || continue
        cmp -s "$DEF" "$GAME/$old" || cp "$GAME/$old" "$GAME/default.txt.bak"
        rm -f "$GAME/$old"
    done
    rm -f "$GAME/wasd-mod.log"
    KEEP=$(active) # the layout in use before the update
    put "$DEF" default.txt
    cmp -s "$EDITOR" "$GAME/Key Layout Editor.html" || cp "$EDITOR" "$GAME/Key Layout Editor.html"
    # It stays in use: a saved layout is marked newer than the fresh default.txt. (Not
    # the newest saved one: a player who picked Default stays on Default.)
    [ -n "$KEEP" ] && [ "$KEEP" != default.txt ] && [ -f "$GAME/$KEEP" ] && touch "$GAME/$KEEP"
    # Wine prefers its own XInput; use the game folder's copy, for this game only.
    wine reg add 'HKCU\Software\Wine\AppDefaults\Dungeons-Win64-Shipping.exe\DllOverrides' \
        /v xinput1_4 /t REG_SZ /d native,builtin /f >/dev/null
    # Option is sent as Alt (hold it for the cursor), and the cursor is confined to
    # the screen area the game asks for rather than one window (CrossOver can pick
    # the mod's small key list instead of the game).
    wine reg add 'HKCU\Software\Wine\Mac Driver' /v UseConfinementCursorClipping /t REG_SZ /d n /f >/dev/null
    for side in Left Right; do
        wine reg add 'HKCU\Software\Wine\Mac Driver' /v ${side}OptionIsAlt /t REG_SZ /d y /f >/dev/null
    done
    echo "layout=$(active)"
    ;;
uninstall)
    needGame; needQuit
    if [ -f "$GAME/xinput1_4.dll" ] && ours "$GAME/xinput1_4.dll"; then rm "$GAME/xinput1_4.dll"; fi
    if [ -f "$GAME/xinput1_4.dll.off" ] && ours "$GAME/xinput1_4.dll.off"; then rm "$GAME/xinput1_4.dll.off"; fi
    [ -f "$GAME/xinput1_4.dll.other" ] && [ ! -f "$GAME/xinput1_4.dll" ] && mv "$GAME/xinput1_4.dll.other" "$GAME/xinput1_4.dll"
    (cd "$GAME" && rm -f default.txt default.txt.bak "Key Layout Editor.html" wasdmod.log wasdmod.old.log wasdmod-record.flag wasdmod.ini wasdmod.ini.bak wasd-mod.ini wasd-mod.ini.bak wasd-mod.log)
    [ "$ALL" = 1 ] && (cd "$GAME" && rm -f author.txt author.txt.bak wasdmod*.txt wasd-mod*.txt)
    wine reg delete 'HKCU\Software\Wine\AppDefaults\Dungeons-Win64-Shipping.exe\DllOverrides' /v xinput1_4 /f >/dev/null 2>&1
    wine reg delete 'HKCU\Software\Wine\Mac Driver' /v UseConfinementCursorClipping /f >/dev/null 2>&1
    for side in Left Right; do
        wine reg delete 'HKCU\Software\Wine\Mac Driver' /v ${side}OptionIsAlt /f >/dev/null 2>&1
    done
    exit 0
    ;;
off)
    # Renamed, not deleted: Wine then uses its own XInput, and the settings and
    # layouts stay. Applies the next time the game starts.
    needGame
    if [ -f "$GAME/xinput1_4.dll" ] && ours "$GAME/xinput1_4.dll"; then
        mv -f "$GAME/xinput1_4.dll" "$GAME/xinput1_4.dll.off" || fail "Couldn't rename the mod's file."
    fi
    echo "state=off"
    ;;
on)
    needGame
    if [ ! -f "$GAME/xinput1_4.dll" ]; then
        [ -f "$GAME/xinput1_4.dll.off" ] && ours "$GAME/xinput1_4.dll.off" || fail "wasdmod isn't installed: click Install."
        mv "$GAME/xinput1_4.dll.off" "$GAME/xinput1_4.dll" || fail "Couldn't rename the mod's file."
    fi
    echo "state=on"
    ;;
use)
    needGame
    case "$2" in
    default) put "$DEF" default.txt ;;
    recommended) put "$REC" author.txt ;;
    own) L=$(own); [ -n "$L" ] || fail "You have no saved layout of your own yet."; touch "$GAME/$L" ;;
    *) fail "use default, recommended or own" ;;
    esac
    echo "layout=$(active)"
    ;;
load)
    needGame
    [ -f "$2" ] || fail "File not found: $2"
    grep -q -e '^\[Buttons\]' -e '^\[Move\]' "$2" || fail "That file isn't a key layout (it has no [Buttons] or [Move] section)."
    # Keeps the editor's names (default.txt, author.txt, wasdmod-MMDDYY.txt).
    NAME=$(basename "$2")
    case "$NAME" in default.txt|author.txt|wasdmod?*.txt) ;; *) NAME=wasdmod.txt ;; esac
    put "$2" "$NAME"
    echo "layout=$(active)"
    ;;
editor)
    needGame
    cmp -s "$EDITOR" "$GAME/Key Layout Editor.html" || cp "$EDITOR" "$GAME/Key Layout Editor.html"
    open "$GAME/Key Layout Editor.html"
    ;;
record)
    # While wasdmod-record.flag is there, the mod logs each key and button it handles.
    needGame
    case "$2" in
    start) : > "$GAME/wasdmod-record.flag" || fail "Couldn't write to the game folder." ;;
    stop) rm -f "$GAME/wasdmod-record.flag" ;;
    *) fail "record start or stop" ;;
    esac
    ;;
report)
    # One text file to attach to a bug report: the system, the mod's state, the
    # layout in use and the log. The home folder shows as ~.
    needGame
    OUT="${2:-$HOME/Desktop/wasdmod-logs-$(date +%Y%m%d-%H%M).txt}"
    L=$(active)
    if [ -f "$GAME/xinput1_4.dll" ]; then ours "$GAME/xinput1_4.dll" && { cmp -s "$DLL" "$GAME/xinput1_4.dll" && ST="installed, up to date" || ST="installed, older version"; } || ST="a different xinput1_4.dll"
    elif [ -f "$GAME/xinput1_4.dll.off" ]; then ST="turned off"; else ST="not installed"; fi
    {
        echo "wasdmod logs, $(date '+%Y-%m-%d %H:%M')"
        echo "App: wasdmod for Mac ${WASDMOD_VERSION:-dev}"
        echo "System: macOS $(sw_vers -productVersion) ($(uname -m))"
        for c in /Applications/CrossOver.app "$HOME/Applications/CrossOver.app"; do
            [ -d "$c" ] && { echo "CrossOver: $(defaults read "$c/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null)"; break; }
        done
        echo "Bottle: $(basename "$BOTTLE")"
        echo "Game folder: $GAME"
        echo "Mod: $ST"
        echo "Layout in use: ${L:-none}"
        if [ -n "$L" ]; then echo; echo "===== $L ====="; grep -v -e '^;' -e '^[[:space:]]*$' "$GAME/$L"; fi
        if [ -f "$GAME/wasdmod.old.log" ]; then echo; echo "===== wasdmod.old.log (end) ====="; tail -n 300 "$GAME/wasdmod.old.log"; fi
        echo; echo "===== wasdmod.log ====="
        if [ -f "$GAME/wasdmod.log" ]; then tail -n 6000 "$GAME/wasdmod.log"; else echo "(no log yet: start the game once with wasdmod installed)"; fi
    } | tr -d '\r' | sed "s|$HOME|~|g" > "$OUT" || fail "Couldn't write $OUT"
    echo "report=$OUT"
    ;;
folder) needGame; open "$GAME" ;;
*) fail "usage: wasdmod.sh status|install|uninstall|off|on|record start|stop|report [FILE]|use default|recommended|own|load FILE|editor|folder" ;;
esac
