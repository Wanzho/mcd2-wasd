#!/usr/bin/env python3
"""wasdmod for Linux and Steam Deck: installs the Minecraft Dungeons II keyboard
controller mod next to the game and opens its key layout editor.

A plain script (Python 3.8 or newer, nothing to install) beside the mod's files:
xinput1_4.dll, default.txt, author.txt, Key Layout Editor.html and lang.json.
Started without a command it does what the Windows and Mac apps do: it finds the
game in Steam's libraries, serves the editor on 127.0.0.1 (a free port, with a
random token in every path so no other page can use it) and opens it in a
browser window. The page has the mod's own buttons (install, disable,
uninstall), the game folder and Troubleshooting; its requests are listed in the
page's first script (configurator.html in the source). The script ends when the
editor's window is closed, or with Ctrl+C.

What it writes: the mod's files in the game folder; one line in the game's own
Proton prefix (see "the Proton prefix" below) and, when the editor is asked to
change the game's keys, the game's keyboard settings file there; its own
settings in ~/.config/wasdmod; the report of Record logs on the Desktop; and a
layout exported from the editor, where the file dialog says. Nothing needs
root, and nothing is downloaded.

Proton uses its own xinput1_4.dll unless it's told to load the game folder's.
Install says so for this game only, in the game's Proton prefix
(steamapps/compatdata/1912410/pfx/user.reg); Uninstall takes it out again. When
that can't be done safely, nothing is changed there and the editor shows the
launch option to paste into Steam instead.

Commands, without the editor (a game folder may follow each one):
  wasdmod.py status      what is installed
  wasdmod.py install     install or update the mod
  wasdmod.py uninstall   uninstall it (saved layouts stay)
  wasdmod.py off | on    turn it off (the game starts without it) or back on
  wasdmod.py find        where the game is
  wasdmod.py --no-open [folder]   the editor's server only: prints its address
For tests: WASDMOD_HOME is another home folder to look in, and with
WASDMOD_TEST_GAME the game counts as running while that file exists.
"""
import base64
import glob
import http.server
import json
import os
import platform
import re
import secrets
import select
import shutil
import socket
import subprocess
import sys
import threading
import time
#if !NEXUS
import urllib.request
#endif

VERSION = "@VERSION@"
#if !NEXUS
UPDATES = "github"
RELEASES_API = "https://api.github.com/repos/Wanzho/mcd2-wasd/releases/latest"
RELEASES_PAGE = "https://github.com/Wanzho/mcd2-wasd/releases/latest"
#else
UPDATES = "nexus"
NEXUS_PAGE = "https://www.nexusmods.com/minecraftdungeons2/mods/104"
#endif
ISSUES_PAGE = "https://github.com/Wanzho/mcd2-wasd/issues/new"
HERE = os.path.dirname(os.path.abspath(__file__))
HOME = os.environ.get("WASDMOD_HOME") or os.path.expanduser("~")
APP_ID = "1912410"  # Minecraft Dungeons II on Steam
EXES = ("Dungeons-Win64-Shipping.exe", "Dungeons-WinGDK-Shipping.exe")
LAUNCH_OPTION = 'WINEDLLOVERRIDES="xinput1_4=n,b" %command%'
ENV_OPTION = "WINEDLLOVERRIDES=xinput1_4=n,b"

# ---------------------------------------------------------------- language
# The text in every language the game has (lang.json, made by lang.py from
# lang/*.json). English is the key: T("Yes") is that text in the current
# language, or the English if it has none. The editor's status line and the
# messages are translated; what the commands print is English.
LANG = "en"
try:
    with open(os.path.join(HERE, "lang.json"), encoding="utf-8") as f:
        LANGS = json.load(f)
except (OSError, ValueError):
    LANGS = {"order": ["en"], "text": {}}


def N_(en):
    """Marks text that T() translates where it's shown."""
    return en


def T(en, **fill):
    text = LANGS.get("text", {}).get(LANG, {}).get(en) or en
    for k, v in fill.items():
        text = text.replace("{%s}" % k, str(v))
    return text


def lang_of(tag):
    """"de", "de_DE.UTF-8", "pt_BR", "zh-Hant", "zh_TW"... -> one of ours, or None."""
    t = tag.split(".")[0].replace("_", "-").lower()
    if t == "zh" or t.startswith("zh-"):
        t = "zh-hant" if t[2:5] in ("-tw", "-hk", "-mo") or t.startswith("zh-hant") else "zh-hans"
    for code in LANGS.get("order", []):
        if t == code.lower() or t.startswith(code.lower() + "-"):
            return code
    return None


def store_dir():
    """Where the editor's layouts are kept (with the game folder picked by hand, and the day of the last update check)."""
    base = None if os.environ.get("WASDMOD_HOME") else os.environ.get("XDG_CONFIG_HOME")
    return os.path.join(base or os.path.join(HOME, ".config"), "wasdmod")


def read_bytes(path, limit=64 << 20):
    try:
        if os.path.getsize(path) > limit:
            return None
        with open(path, "rb") as f:
            return f.read()
    except OSError:
        return None


def read_text(path, limit=64 << 20):
    """A file as text with every byte as it is (one character each), or None."""
    b = read_bytes(path, limit)
    return None if b is None else b.decode("latin-1")


def write_file(path, data):
    """Writes a file whole: as a new file beside it, which then takes its place (never half written)."""
    tmp = path + ".wasdmod-new"
    try:
        with open(tmp, "wb") as f:
            f.write(data)
        os.replace(tmp, path)
        return True
    except OSError:
        remove(tmp)
        return False


def remove(path):
    try:
        os.remove(path)
        return True
    except OSError:
        return False


def keep(name, data):
    """Writes one of this script's own files (in ~/.config/wasdmod, made when first needed)."""
    try:
        os.makedirs(store_dir(), exist_ok=True)
    except OSError:
        return False
    return write_file(os.path.join(store_dir(), name), data)


def pick_language():
    """The language picked in the key layout editor (kept with its layouts), else the system's."""
    global LANG
    saved = read_text(os.path.join(store_dir(), "editor.json")) or ""
    m = re.search(r'"d2kb-lang":"([^"]*)"', saved)
    code = lang_of(m.group(1)) if m else None
    for name in ("LC_ALL", "LC_MESSAGES", "LANGUAGE", "LANG"):
        if not code and os.environ.get(name):
            code = lang_of(os.environ[name].split(":")[0])
    LANG = code or "en"


# ---------------------------------------------------------------- finding the game

SUBS = ("", "Dungeons/Binaries/Win64", "Dungeons/Binaries/WinGDK", "Content/Dungeons/Binaries/Win64",
        "Content/Dungeons/Binaries/WinGDK", "Binaries/Win64", "Binaries/WinGDK", "Win64", "WinGDK")


def is_game_dir(d):
    return any(os.path.isfile(os.path.join(d, e)) for e in EXES)


def game_at(root):
    for sub in SUBS:
        d = os.path.join(root, sub) if sub else root
        if is_game_dir(d):
            return d
    return None


def dir_from_pick(pick):
    """The game folder for a picked folder or exe: itself, or a nearby parent's (no search of the disk)."""
    root = os.path.abspath(pick.rstrip("/") or "/")
    if os.path.isfile(root):
        if not root.lower().endswith(".exe"):
            return None
        root = os.path.dirname(root)
    elif not os.path.isdir(root):
        return None
    for _ in range(5):
        found = game_at(root)
        if found:
            return found
        if os.path.dirname(root) == root:
            break
        root = os.path.dirname(root)
    return None


def steam_roots():
    """Steam's own folders: the usual ones, and the Flatpak's and the Snap's."""
    places = (".steam/steam", ".steam/root", ".local/share/Steam", ".steam/debian-installation",
              ".var/app/com.valvesoftware.Steam/.local/share/Steam", ".var/app/com.valvesoftware.Steam/.steam/steam",
              ".var/app/com.valvesoftware.Steam/data/Steam", "snap/steam/common/.local/share/Steam", "snap/steam/common/.steam/steam")
    roots = []
    for p in places:
        d = os.path.realpath(os.path.join(HOME, p))
        if os.path.isdir(os.path.join(d, "steamapps")) and d not in roots:
            roots.append(d)
    return roots


def vdf_text(value):
    """A quoted value of one of Steam's .vdf files, as text."""
    return re.sub(r"\\(.)", r"\1", value).encode("latin-1").decode("utf-8", "replace")


def libraries(root):
    """Steam's library list: every "path" "/run/media/..." entry in libraryfolders.vdf (the Deck's SD card too)."""
    libs = [root]
    for vdf in ("steamapps/libraryfolders.vdf", "config/libraryfolders.vdf"):
        text = read_text(os.path.join(root, vdf), 4 << 20) or ""
        for m in re.finditer(r'"path"\s+"((?:[^"\\]|\\.)*)"', text, re.I):
            lib = os.path.realpath(vdf_text(m.group(1)))
            if lib not in libs:
                libs.append(lib)
    return libs


def find_game():
    for root in steam_roots():
        for lib in libraries(root):
            names = ["Minecraft Dungeons II", "Minecraft Dungeons 2"]
            manifest = read_text(os.path.join(lib, "steamapps", "appmanifest_%s.acf" % APP_ID), 1 << 20) or ""
            m = re.search(r'"installdir"\s+"((?:[^"\\]|\\.)*)"', manifest, re.I)
            if m:  # (the folder's name, as Steam has it; only its last part is used)
                names.insert(0, os.path.basename(vdf_text(m.group(1))) or names[0])
            for name in names:
                found = game_at(os.path.join(lib, "steamapps", "common", name))
                if found:
                    return found
    return None


def own_processes():
    """This script and what started it (the launcher, the terminal): their numbers."""
    pids, pid = set(), os.getpid()
    while pid > 1 and pid not in pids:
        pids.add(pid)
        m = re.search(r"^PPid:\s*(\d+)", read_text("/proc/%d/status" % pid, 1 << 16) or "", re.M)
        pid = int(m.group(1)) if m else 0
    return pids


def game_running():
    marker = os.environ.get("WASDMOD_TEST_GAME")  # tests: "running" while that file exists
    if marker:
        return os.path.exists(marker)
    names = [e.lower().encode() for e in EXES]
    ours = own_processes()  # (a game folder given to this script has the exe's name in it too)
    for path in glob.glob("/proc/[0-9]*/cmdline"):
        if int(os.path.basename(os.path.dirname(path))) in ours:
            continue
        try:
            with open(path, "rb") as f:
                line = f.read(1 << 16).lower()
        except OSError:
            continue
        if any(n in line for n in names):
            return True
    return False


# ---------------------------------------------------------------- the Proton prefix
#
# Wine (Proton) loads its own xinput1_4.dll unless the registry value
#   HKCU\Software\Wine\AppDefaults\<the game's exe>\DllOverrides  "xinput1_4"="native,builtin"
# says to try the one in the game folder first. That is what the Mac app sets with
# CrossOver's wine (mac/wasdmod.sh), for this game only. Here there is no wine to ask:
# the value goes straight into the prefix's registry file, user.reg, as one line of
# text (in a new key at the end of the file when the game has none yet). The rules:
#   - only into a file that looks as Wine writes it (its first two lines, Unix line
#     ends, no symbolic link), and only when it says nothing about xinput1_4 yet;
#   - only while the game isn't running and no wineserver has the prefix open (Wine
#     writes the file again when it ends, from what it has in memory);
#   - the file as it was is kept beside it as user.reg.wasdmod-backup; the new text is
#     the old with the line put in and nothing else touched, written to a new file
#     that takes the old one's place, and read again afterwards;
#   - Uninstall takes out that line (and the key, if nothing else is in it) and the
#     backup. A line that is there without our backup is someone's own: it stays.
# Anything else: nothing is changed, and the editor shows the launch option to paste
# into Steam, which does the same.
OVERRIDE = '"xinput1_4"="native,builtin"'
ALL_APPS_KEY = "[Software\\\\Wine\\\\DllOverrides]"


def prefix_of(game):
    """The game's Proton prefix, when the game is in a Steam library and has run once."""
    d = game
    while os.path.dirname(d) != d:
        if os.path.basename(d) == "common" and os.path.basename(os.path.dirname(d)) == "steamapps":
            pfx = os.path.join(os.path.dirname(d), "compatdata", APP_ID, "pfx")
            if os.path.isdir(pfx):
                return pfx
            for root in steam_roots():  # (Steam may keep the prefix in another library than the game)
                for lib in libraries(root):
                    pfx = os.path.join(lib, "steamapps", "compatdata", APP_ID, "pfx")
                    if os.path.isdir(pfx):
                        return pfx
            return None
        d = os.path.dirname(d)
    return None


def in_steam(game):
    return "/steamapps/common/" in game.replace(os.sep, "/") + "/"


def game_exe(game):
    return next((e for e in EXES if os.path.isfile(os.path.join(game, e))), EXES[0])


def app_key(game):
    return "[Software\\\\Wine\\\\AppDefaults\\\\%s\\\\DllOverrides]" % game_exe(game)


def reg_load(pfx):
    """user.reg as text, or None when it isn't the file as Wine writes it."""
    path = os.path.join(pfx, "user.reg")
    text = None if os.path.islink(path) else read_text(path)
    if not text or not text.startswith("WINE REGISTRY Version 2\n;; All keys relative to ") or "\\\\User\\\\" not in text.split("\n", 2)[1]:
        return None
    if "\r" in text or "\0" in text or not text.endswith("\n"):
        return None
    return text


def reg_section(text, key):
    """Where a key's section is: (the start of its "[...]" line, the start of the next key or the end), or None."""
    m = re.search(r"^%s(?= |$)" % re.escape(key), text, re.M | re.I)  # (Wine's key names don't mind the case)
    if not m:
        return None
    end = text.find("\n[", m.start())
    return m.start(), (len(text) if end < 0 else end + 1)


def reg_says(text, key):
    """What a key says about xinput1_4: None (nothing), "ours" (exactly the line Install writes),
    "native" (another setting that tries the game folder's file first), or "other"."""
    span = reg_section(text, key)
    said = None
    for line in text[span[0]:span[1]].split("\n")[1:] if span else []:
        if line.split("=", 1)[0].lower() in ('"xinput1_4"', '"*xinput1_4"'):
            kind = "ours" if line == OVERRIDE else "native" if line.split("=", 1)[1].lower().startswith('"n') else "other"
            said = kind if said in (None, kind) else "other"  # (said twice, differently: not ours to judge)
    return said


def reg_loads_mod(text, game):
    """The registry tells Wine to load the game folder's file: the game's own key decides, then the key for every program."""
    own = reg_says(text, app_key(game))
    return own in ("ours", "native") or (own is None and reg_says(text, ALL_APPS_KEY) in ("ours", "native"))


def reg_with(text, key):
    """The text with our line: under the key's own lines, or with the key in a new section at the end."""
    span = reg_section(text, key)
    if span:
        at = text.index("\n", span[0]) + 1
        while text.startswith("#", at):  # (#time=...)
            at = text.index("\n", at) + 1
        return text[:at] + OVERRIDE + "\n" + text[at:]
    now = int(time.time())
    return text + "\n%s %d\n#time=%x\n%s\n" % (key, now, (now + 11644473600) * 10000000, OVERRIDE)


def reg_without(text, key):
    """The text without our line, and without the key's section when nothing else is in it."""
    start, end = reg_section(text, key)
    lines = text[start:end].split("\n")
    if any(l and not l.startswith("#") and l != OVERRIDE for l in lines[1:]):  # other values: only the line goes
        at = text.index("\n" + OVERRIDE + "\n", start, end + 1) + 1
        return text[:at] + text[at + len(OVERRIDE) + 1:]
    if end == len(text) and text[:start].endswith("\n\n"):  # the last section: the empty line before it goes with it
        start -= 1
    return text[:start] + text[end:]


def prefix_busy(pfx):
    """Wine has the prefix open (its registry file is Wine's until it ends)."""
    if game_running():
        return True
    for path in glob.glob("/proc/[0-9]*/comm"):
        try:
            with open(path, "rb") as f:
                if not f.read().startswith(b"wineserver"):
                    continue
        except OSError:
            continue
        try:
            with open(os.path.join(os.path.dirname(path), "environ"), "rb") as f:
                env = f.read().split(b"\0")
        except OSError:
            return True  # a wineserver that can't be asked which prefix it has: it may be this one
        if any(v.startswith(b"WINEPREFIX=") and os.path.realpath(v[11:].decode("utf-8", "replace")) == os.path.realpath(pfx) for v in env):
            return True
    return False


def reg_write(pfx, new, old):
    """user.reg replaced by `new` (it was `old` when it was read); True when that is what's there afterwards."""
    path = os.path.join(pfx, "user.reg")
    tmp = path + ".wasdmod-new"
    try:
        if read_text(path) != old:  # changed since it was read
            return False
        with open(tmp, "wb") as f:
            f.write(new.encode("latin-1"))
            f.flush()
            os.fsync(f.fileno())
        shutil.copymode(path, tmp)
        os.replace(tmp, path)
        return read_text(path) == new
    except OSError:
        remove(tmp)
        return False


def prefix_free(pfx):
    """Nothing of Wine's has the prefix open. (Wine stays for a few seconds after the game has closed: waited for.)"""
    for _ in range(10):
        if not prefix_busy(pfx):
            return True
        if game_running():
            break
        time.sleep(0.5)
    return False


def override_set(game):
    """Tells Proton to load the game folder's xinput1_4.dll. True when it's said (now, or already)."""
    pfx = prefix_of(game)
    text = reg_load(pfx) if pfx else None
    if not text:
        return False
    if reg_loads_mod(text, game):
        return True
    key = app_key(game)
    if reg_says(text, key) or reg_says(text, ALL_APPS_KEY) or not prefix_free(pfx):  # (someone's own setting is left alone)
        return False
    text = reg_load(pfx)  # (read again: Wine writes it once more as it ends)
    if not text or reg_says(text, key) or reg_says(text, ALL_APPS_KEY):
        return False
    new = reg_with(text, key)
    if reg_says(new, key) != "ours" or reg_without(new, key) != text:  # (taking it out again must give the file as it is)
        return False
    backup = os.path.join(pfx, "user.reg.wasdmod-backup")
    try:
        shutil.copy2(os.path.join(pfx, "user.reg"), backup)
    except OSError:
        return False
    if read_text(backup) == text and reg_write(pfx, new, text):
        return True
    remove(backup)
    return False


def override_clear(game):
    """Takes our line out again (Uninstall). A line without our backup beside the file is someone's own: it stays."""
    pfx = prefix_of(game)
    text = reg_load(pfx) if pfx else None
    backup = os.path.join(pfx, "user.reg.wasdmod-backup") if pfx else ""
    key = app_key(game)
    if not text or reg_says(text, key) != "ours" or not os.path.isfile(backup) or not prefix_free(pfx):
        return
    text = reg_load(pfx)
    if not text or reg_says(text, key) != "ours":
        return
    new = reg_without(text, key)
    if reg_says(new, key) is None and len(new) < len(text) and reg_write(pfx, new, text):
        remove(backup)


def launch_option_set():
    """The launch option is in Steam already (pasted by hand): Steam's own settings say so."""
    for root in steam_roots():
        for path in glob.glob(os.path.join(root, "userdata", "*", "config", "localconfig.vdf")):
            text = read_text(path, 32 << 20) or ""
            for m in re.finditer(r'"%s"\s*\{' % APP_ID, text):
                depth, at = 1, m.end()
                while depth and 0 <= at < len(text):  # to the app's closing brace
                    if text[at] == '"':
                        at = text.find('"', at + 1)
                        while at > 0 and text[at - 1] == "\\":
                            at = text.find('"', at + 1)
                        if at < 0:
                            break
                    elif text[at] in "{}":
                        depth += 1 if text[at] == "{" else -1
                    at += 1
                o = re.search(r'"LaunchOptions"\s+"((?:[^"\\]|\\.)*)"', text[m.end():at if at > 0 else len(text)], re.I)
                if o and "xinput1_4" in o.group(1) and "WINEDLLOVERRIDES" in o.group(1):
                    return True
    return False


def override_ok(game):
    pfx = prefix_of(game)
    text = reg_load(pfx) if pfx else None
    return bool(text and reg_loads_mod(text, game)) or launch_option_set()


# ---------------------------------------------------------------- install / uninstall

OK, ERR_RUNNING, ERR_OTHER_DLL, ERR_WRITE = range(4)
NOT_INSTALLED, INSTALLED_LATEST, INSTALLED_OLDER, OTHER_DLL, TURNED_OFF = range(5)
STATE_NAMES = ("not installed", "installed, up to date", "installed, older version", "a different xinput1_4.dll is there", "turned off")


def error_text(code):
    return (None,
            T("Minecraft Dungeons II is running. Close it, then try again."),
            T("The game folder already has a different xinput1_4.dll (another mod?)."),
            T("Couldn't write to the game folder."))[code]


def payload(name):
    return read_bytes(os.path.join(HERE, name)) or b""


def is_our_dll(path):
    b = read_bytes(path)
    return bool(b) and (b"Controller mod loaded" in b or b"WASD mod loaded" in b)


def install_state(game):
    dll, off = os.path.join(game, "xinput1_4.dll"), os.path.join(game, "xinput1_4.dll.off")
    if not os.path.exists(dll):
        return TURNED_OFF if is_our_dll(off) else NOT_INSTALLED
    if not is_our_dll(dll):
        return OTHER_DLL
    return INSTALLED_LATEST if read_bytes(dll) == payload("xinput1_4.dll") else INSTALLED_OLDER


def mod_version(game):
    """The version in the game folder's mod (" wasdmod 1.3.1, " in its log line), or "" for versions before that line."""
    b = read_bytes(os.path.join(game, "xinput1_4.dll")) or read_bytes(os.path.join(game, "xinput1_4.dll.off")) or b""
    m = re.search(rb" wasdmod ([0-9][0-9.]*),", b, re.I)
    return m.group(1).decode() if m else ""


def named(game, start, end=".txt"):
    """The game folder's files whose names start and end so, whatever the case."""
    try:
        names = os.listdir(game)
    except OSError:
        return []
    return [os.path.join(game, n) for n in names if n.lower().startswith(start) and n.lower().endswith(end)]


def layouts(game):
    """Saved key layouts: author.txt and the editor's wasdmod-MMDDYY.txt files."""
    return named(game, "author.txt") + named(game, "wasdmod")


def mtime(path):
    try:
        return os.stat(path).st_mtime_ns
    except OSError:
        return -1


def active_settings(game):
    """The settings file the mod uses: the newest saved layout when it's newer than default.txt."""
    ini = os.path.join(game, "default.txt")
    saved = max(layouts(game), key=mtime, default=None)
    return saved if saved and mtime(saved) > mtime(ini) else ini


def put_layout(game, name, data):
    """Writes a layout, which makes it the newest file and so the one the mod uses; an edited copy is kept as NAME.bak."""
    path = os.path.join(game, name)
    if os.path.exists(path) and read_bytes(path) != data:
        try:
            shutil.copyfile(path, path + ".bak")
        except OSError:
            pass
    return write_file(path, data)


def install(game, replace_other=False):
    if game_running():
        return ERR_RUNNING
    dll = os.path.join(game, "xinput1_4.dll")
    if os.path.exists(dll) and not is_our_dll(dll):
        if not replace_other:
            return ERR_OTHER_DLL
        try:
            shutil.copyfile(dll, dll + ".other")
        except OSError:
            return ERR_WRITE
    if not payload("xinput1_4.dll") or not write_file(dll, payload("xinput1_4.dll")):
        return ERR_WRITE
    if is_our_dll(dll + ".off"):  # installing turns it back on
        remove(dll + ".off")
    # Older versions named everything wasd-mod...: a saved layout keeps its date and its time stamp.
    for old in named(game, "wasd-mod"):
        try:
            os.replace(old, os.path.join(game, "wasdmod" + os.path.basename(old)[8:]))
        except OSError:
            pass
    # The settings file used to be wasd-mod.ini, then wasdmod.ini; edited ones are kept as default.txt.bak.
    for name in ("wasd-mod.ini", "wasdmod.ini"):
        old = os.path.join(game, name)
        if os.path.exists(old):
            if read_bytes(old) != payload("default.txt"):
                try:
                    shutil.copyfile(old, os.path.join(game, "default.txt.bak"))
                except OSError:
                    pass
            remove(old)
    remove(os.path.join(game, "wasd-mod.log"))
    was = active_settings(game)  # the layout in use before the update
    if not put_layout(game, "default.txt", payload("default.txt")):
        return ERR_WRITE
    write_file(os.path.join(game, "Key Layout Editor.html"), payload("Key Layout Editor.html"))
    # It stays in use: a saved layout is marked newer than the fresh default.txt (two seconds
    # newer, if "now" isn't later than that file's time on this disk).
    if os.path.basename(was).lower() != "default.txt" and os.path.exists(was):
        try:
            os.utime(was, None)
            fresh = mtime(os.path.join(game, "default.txt"))
            if mtime(was) <= fresh:
                os.utime(was, ns=(fresh + 2 * 10 ** 9, fresh + 2 * 10 ** 9))
        except OSError:
            pass
    override_set(game)  # (when it can't be said there, the status line has the launch option)
    return OK


def uninstall(game, remove_layouts=False):
    if game_running():
        return ERR_RUNNING
    dll = os.path.join(game, "xinput1_4.dll")
    if is_our_dll(dll) and not remove(dll):
        return ERR_WRITE
    if is_our_dll(dll + ".off"):
        remove(dll + ".off")
    gone = ["default.txt", "default.txt.bak", "wasdmod.log", "wasdmod.old.log", "wasdmod-record.flag", "Key Layout Editor.html",
            "wasdmod.ini", "wasdmod.ini.bak", "wasd-mod.ini", "wasd-mod.ini.bak", "wasd-mod.log"]
    if remove_layouts:
        gone += [os.path.basename(p) for p in layouts(game) + named(game, "wasd-mod")] + ["author.txt.bak"]
    for name in gone:
        remove(os.path.join(game, name))
    override_clear(game)
    return OK


def turn_on(game, on):
    """Turning the mod off renames it to xinput1_4.dll.off: the game starts without it, and the layouts stay.
    Works with the game running too (it applies the next time the game starts)."""
    dll = os.path.join(game, "xinput1_4.dll")
    try:
        if on:
            if not os.path.exists(dll):
                if not is_our_dll(dll + ".off"):
                    return ERR_WRITE
                os.rename(dll + ".off", dll)
        elif is_our_dll(dll):
            os.replace(dll, dll + ".off")
    except OSError:
        return ERR_WRITE
    return OK


# ---------------------------------------------------------------- Record Logs
#
# While wasdmod-record.flag is in the game folder, the mod logs each key and mouse
# button it handles. Stop and save logs writes one text file to the Desktop for a
# bug report: the system, the mod's state, the layout in use and the log.

def desktop():
    d = ""
    if not os.environ.get("WASDMOD_HOME") and shutil.which("xdg-user-dir"):
        try:
            d = subprocess.run(["xdg-user-dir", "DESKTOP"], capture_output=True, universal_newlines=True, timeout=5).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            d = ""
    if not d or not os.path.isdir(d):
        d = os.path.join(HOME, "Desktop")
    return d if os.path.isdir(d) else HOME


def tail(path, keep, settings_only=False):
    """The end of a file: at most `keep` bytes, from the start of a line; with settings_only, without comments."""
    b = read_bytes(path) or b""
    if len(b) > keep:
        b = b[len(b) - keep:]
        b = b[b.find(b"\n") + 1:]
    lines = b.decode("utf-8", "replace").replace("\r", "").split("\n")
    if settings_only:
        lines = [l for l in lines if l.strip() and not l.startswith(";")]
    return "\n".join(lines).rstrip("\n") + "\n"


def write_report(game):
    """The report's file on the Desktop, or None."""
    now = time.localtime()
    out = os.path.join(desktop(), time.strftime("wasdmod-logs-%Y%m%d-%H%M.txt", now))
    system = "Linux " + platform.release()
    m = re.search(r'^PRETTY_NAME="?([^"\n]*)', read_text("/etc/os-release", 1 << 16) or "", re.M)
    if m:
        system += " (%s)" % m.group(1)
    active = active_settings(game)
    pfx = prefix_of(game)
    text = time.strftime("wasdmod logs, %Y-%m-%d %H:%M\n", now)
    text += "App: wasdmod for Linux %s\nSystem: %s\n" % (VERSION, system)
    text += "Game folder: %s\nMod: %s\n" % (game, STATE_NAMES[install_state(game)].replace(" is there", ""))
    text += "Proton prefix: %s; told to load the mod: %s\n" % ("found" if pfx else "not found", "yes" if override_ok(game) else "no")
    text += "Layout in use: %s\n" % (os.path.basename(active) if os.path.exists(active) else "none")
    if os.path.exists(active):
        text += "\n===== %s =====\n%s" % (os.path.basename(active), tail(active, 64 << 10, True))
    if os.path.exists(os.path.join(game, "wasdmod.old.log")):
        text += "\n===== wasdmod.old.log (end) =====\n" + tail(os.path.join(game, "wasdmod.old.log"), 32 << 10)
    text += "\n===== wasdmod.log =====\n"
    if os.path.exists(os.path.join(game, "wasdmod.log")):
        text += tail(os.path.join(game, "wasdmod.log"), 1200 << 10)
    else:
        text += "(no log yet: start the game once with wasdmod installed)\n"
    for home in sorted({os.path.realpath(HOME), HOME, os.path.expanduser("~")}, key=len, reverse=True):  # no user name in the report
        if len(home) > 1:
            text = text.replace(home, "~")
    return out if write_file(out, text.encode("utf-8")) else None


# ---------------------------------------------------------------- dialogs
# Messages and file dialogs come from zenity or kdialog (the Deck has kdialog). With
# neither, there are none: the editor's status line and the terminal say what happened.

def dialog_tool():
    kde = "kde" in os.environ.get("XDG_CURRENT_DESKTOP", "").lower()
    for tool in (("kdialog", "zenity") if kde else ("zenity", "kdialog")):
        if shutil.which(tool):
            return tool
    return None


def run_dialog(args):
    try:
        r = subprocess.run(args, capture_output=True, universal_newlines=True)
        return r.returncode, r.stdout.strip()
    except OSError:
        return 1, ""


def tell(line):
    """A line for the terminal, when there is one."""
    try:
        print(line, flush=True)
    except (OSError, ValueError):
        pass


def say(text, warning=False):
    tool = dialog_tool()
    if tool == "zenity":
        run_dialog(["zenity", "--warning" if warning else "--info", "--title", "wasdmod", "--no-markup", "--width", "420", "--text", text])
    elif tool == "kdialog":
        run_dialog(["kdialog", "--title", "wasdmod", "--sorry" if warning else "--msgbox", text])
    else:
        tell(text)


def ask(text, cancel=False):
    """A question: "yes", "no", or (with cancel) "cancel". Without a dialog tool: "no"."""
    tool = dialog_tool()
    if tool == "zenity":
        args = ["zenity", "--question", "--title", "wasdmod", "--no-markup", "--width", "420", "--text", text]
        if not cancel:
            return "yes" if run_dialog(args)[0] == 0 else "no"
        # (closing the window is Cancel: No is the extra button, which zenity answers with its name)
        code, out = run_dialog(args + ["--ok-label", T("Yes"), "--cancel-label", T("Cancel"), "--extra-button", T("No")])
        return "yes" if code == 0 else "no" if out == T("No") else "cancel"
    if tool == "kdialog":
        code, _ = run_dialog(["kdialog", "--title", "wasdmod", "--yesnocancel" if cancel else "--yesno", text])
        return "yes" if code == 0 else "no" if code == 1 else "cancel" if cancel else "no"
    return "no"


def pick_file(title, start):
    tool = dialog_tool()
    if tool == "zenity":
        code, out = run_dialog(["zenity", "--file-selection", "--title", title, "--filename", start.rstrip("/") + "/"])
    elif tool == "kdialog":
        code, out = run_dialog(["kdialog", "--title", title, "--getopenfilename", start])
    else:
        return None
    return out if code == 0 and out else None


def save_file(name):
    tool = dialog_tool()
    start = os.path.join(desktop(), name)
    if tool == "zenity":
        code, out = run_dialog(["zenity", "--file-selection", "--save", "--confirm-overwrite", "--filename", start])
    elif tool == "kdialog":
        code, out = run_dialog(["kdialog", "--getsavefilename", start])
    else:
        return None
    return out if code == 0 and out else None


def open_path(what):
    """A folder in the file manager, or a web page in the browser."""
    if shutil.which("xdg-open"):
        subprocess.Popen(["xdg-open", what], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


# ---------------------------------------------------------------- the mod's state, for the editor page
#
# status when the page loads, then on every change, as in the apps: text is the status
# line; state is none (also another mod's file), older, current or off, for the page's
# mod buttons; found is false without a game folder; busy while one of the page's
# buttons is at work.
game = None  # the game folder
busy = False
status_lock = threading.Condition()
status_now, status_seq = "", 0
NOT_FOUND = N_("Game not found in Steam. Under Game folder, click Choose game folder… and pick the game's Dungeons-Win64-Shipping.exe.")


def older_text():
    v, running = mod_version(game), game_running()
    if not v:
        return T("The mod in the game is an older version. Quit the game, then click Update Mod.") if running \
            else T("The mod in the game is an older version. Click Update Mod to update it.")
    return T("The mod in the game is still {version}; this app is {app}. Quit the game, then click Update Mod.", version=v, app=VERSION) if running \
        else T("The mod in the game is still {version}; this app is {app}. Click Update Mod to update it.", version=v, app=VERSION)


def to_json(value):
    return json.dumps(value, ensure_ascii=False).replace("<", "\\u003c")  # (< so it can't end a script)


def status_json():
    state = install_state(game) if game else NOT_INSTALLED
    if not game:
        text = T(NOT_FOUND)
    elif state == INSTALLED_LATEST and override_ok(game):
        text = T("Installed and up to date. Start (or restart) the game to use it.")
    elif state == INSTALLED_LATEST and in_steam(game):
        text = T("Installed, but Proton still loads its own xinput1_4.dll. In Steam, open the game's Properties and paste this into Launch Options: {option}", option=LAUNCH_OPTION)
    elif state == INSTALLED_LATEST:
        text = T("Installed. For the game to load it, add this environment variable in your launcher's settings for the game: {option}", option=ENV_OPTION)
    elif state == INSTALLED_OLDER:
        text = older_text()
    elif state == OTHER_DLL:
        text = T("The game folder has a different xinput1_4.dll (another mod?). Install Mod replaces it and keeps a copy.")
    elif state == TURNED_OFF:
        text = T("Disabled: the game starts without wasdmod. Your layouts are kept; click Enable to use it again.")
    else:
        text = T("Not installed yet. Quit the game, then click Install Mod.")
    return to_json({"text": text, "recording": bool(game) and os.path.exists(os.path.join(game, "wasdmod-record.flag")), "found": bool(game),
                    "state": {INSTALLED_OLDER: "older", INSTALLED_LATEST: "current", TURNED_OFF: "off"}.get(state, "none"), "busy": busy})


def status_changed():
    """Looked at again after anything that may have changed it: a new state gets the next number and wakes the pages that wait."""
    global status_now, status_seq
    with status_lock:  # (one look at a time, so an older look can't be the last word)
        now = status_json()
        if now != status_now or not status_seq:
            status_now, status_seq = now, status_seq + 1
            status_lock.notify_all()


# ---------------------------------------------------------------- the editor page's own buttons

def run_install():
    r = install(game)
    if r == ERR_OTHER_DLL and dialog_tool():
        if ask(T("The game folder already has a different xinput1_4.dll (probably another mod). Replace it? A copy is kept as xinput1_4.dll.other.")) != "yes":
            return None
        r = install(game, True)
    status_changed()
    if r != OK:
        if dialog_tool():
            say(error_text(r), True)
        return error_text(r)
    if dialog_tool():
        say(T("Installed. Start (or restart) Minecraft Dungeons II.\n\nIn game: WASD moves, Tab opens the menu wheel (the game's own key, S, moves you now), F9 shows the key list, and the backtick key (`) turns the mod off and on."))
    return None


def run_toggle():
    on = install_state(game) == TURNED_OFF
    r = turn_on(game, on)
    status_changed()
    if r != OK:
        failed = T("Couldn't rename xinput1_4.dll in the game folder. Close the game and try again.")
        if dialog_tool():
            say(failed, True)
        return failed
    if dialog_tool():
        say(T("Enabled. Start (or restart) the game to use wasdmod again.") if on
            else T("Disabled. From the next game start, the game runs without wasdmod; your layouts are kept.\n\n(In a running game, the backtick key (`) turns it off right away.)"))
    return None


def run_uninstall():
    """(Asked about the saved layouts first, when there are any: Cancel leaves the mod installed.)"""
    if game_running():  # (said before the question: nothing would be removed anyway)
        if dialog_tool():
            say(error_text(ERR_RUNNING), True)
        return error_text(ERR_RUNNING)
    answer = ask(T("Also delete your saved key layouts (author.txt, wasdmod*.txt)?"), cancel=True) if layouts(game) and dialog_tool() else "no"
    if answer == "cancel":
        return None
    r = uninstall(game, answer == "yes")
    status_changed()
    if r != OK:
        if dialog_tool():
            say(error_text(r), True)
        return error_text(r)
    if dialog_tool():
        say(T("Uninstalled. The game is back to how it was."))
    return None


def fail(text):
    """Something that went wrong, in a message; without a dialog tool it's the request's answer, which the page shows."""
    if not dialog_tool():
        raise Failed(text)
    say(text, True)


def run_record():
    flag = os.path.join(game, "wasdmod-record.flag")
    if not os.path.exists(flag):
        ok = write_file(flag, b"")
        status_changed()
        if not ok:
            return fail(error_text(ERR_WRITE))
        if dialog_tool():
            say(T("Recording logs. Play until the problem happens, then come back here and click Stop and save logs.\n\nIf the game is running, recording starts within a second; otherwise it starts with the game. Nothing you type is recorded."))
        return
    remove(flag)
    status_changed()
    out = write_report(game)
    if not out:
        return fail(T("Couldn't save the logs on the Desktop."))
    tell(out)
    open_path(os.path.dirname(out))  # the folder it is in, in the file manager
    if ask(T("Saved {file} on your Desktop. It has the mod's log, your key layout and your system's version; nothing you type is recorded.\n\nOpen GitHub to report the problem (attach the file)?", file=os.path.basename(out))) == "yes":
        open_path(ISSUES_PAGE)


def choose_folder():
    global game
    if not dialog_tool():
        raise Failed(T("No file dialog was found (zenity or kdialog). Start wasdmod-linux.sh with the game's folder after its name instead."))
    start = next((os.path.join(lib, "steamapps", "common") for root in steam_roots() for lib in libraries(root)
                  if os.path.isdir(os.path.join(lib, "steamapps", "common"))), HOME)
    pick = pick_file(T("Find Minecraft Dungeons II"), start)
    if not pick:
        return "cancelled"
    found = dir_from_pick(pick)
    if not found:
        say(T("That isn't the Minecraft Dungeons II folder. Pick Dungeons*.exe (in Dungeons\\Binaries) or Dungeons.exe."), True)
        return "cancelled"
    game = found
    keep("game-folder", found.encode("utf-8", "surrogateescape"))  # for the next start
    return "done"


def export_file(name, body):
    """Export... in the page: a save dialog for the layout's file. Fails with the system's own words."""
    name = re.sub(r'[\\/:*?"<>|\x00-\x1f]', "-", name.replace("\\", "/").rsplit("/", 1)[-1]).lstrip(".-") or "wasdmod.txt"
    out = save_file(name)
    if not out:
        return "cancelled"
    try:
        with open(out, "wb") as f:
            f.write(body)
    except OSError as e:
        raise Failed(e.strerror or str(e))
    return "saved"


#if !NEXUS
def newest_release():
    """The newest version on GitHub ("1.4.1"), or None when GitHub can't be asked. One request, with
    nothing about this computer in it; nothing is downloaded but its answer, which is only read."""
    keep("last-update-check", VERSION.encode())  # asked today
    try:
        req = urllib.request.Request(RELEASES_API, headers={"Accept": "application/vnd.github+json", "User-Agent": "wasdmod-linux/" + VERSION})
        with urllib.request.urlopen(req, timeout=15) as r:
            tag = str(json.loads(r.read(4 << 20).decode("utf-8"))["tag_name"])
    except Exception:  # (no connection, or not the answer expected: nothing is said about it)
        return None
    return tag[1:] if tag[:1] in ("v", "V") else tag


def newer(a, b):
    """"1.10.0" > "1.9.2"; a version that isn't numbers is never newer."""
    if not re.fullmatch(r"[0-9]+(\.[0-9]+)*", a) or not re.match(r"[0-9]", b):
        return False
    x, y = [int(n) for n in a.split(".")], [int(n) for n in re.findall(r"\d+", b)]
    longest = max(len(x), len(y))
    return x + [0] * (longest - len(x)) > y + [0] * (longest - len(y))


def tell_update(version):
    """A newer version: said, with the way to its download page (nothing is downloaded here)."""
    tell(T("wasdmod {version} is available", version=version) + ": " + RELEASES_PAGE)
    if dialog_tool() and ask(T("wasdmod {version} is available. Open its download page?", version=version)) == "yes":
        open_path(RELEASES_PAGE)


def daily_check():
    """At most once a day, once the editor has opened: only tells, and says nothing when GitHub can't be reached."""
    last = os.path.join(store_dir(), "last-update-check")
    if 0 <= time.time() - mtime(last) / 1e9 < 86400:
        return
    while not page_seen:
        time.sleep(1)
    time.sleep(3)
    version = newest_release()
    if version and newer(version, VERSION):
        tell_update(version)
#endif


def check_updates():
    """Check for updates in the page. Answers with a line for the page to show, when no dialog said it."""
#if !NEXUS
    version = newest_release()
    if version is None:
        said = T("Couldn't reach GitHub. Check the internet connection and try again.")
    elif not newer(version, VERSION):
        said = T("wasdmod {version} is the newest version.", version=VERSION)
    elif dialog_tool():
        tell_update(version)
        return ""
    else:
        open_path(RELEASES_PAGE)
        return T("wasdmod {version} is available", version=version)
    if not dialog_tool():
        return said
    say(said)
    return ""
#else
    open_path(NEXUS_PAGE)  # the browser; nothing is downloaded here
    return ""
#endif


class Failed(Exception):
    """A request that failed, with the reason for the page."""


def page_call(what, arg="", body=b""):
    """One of the page's buttons, one at a time: the answer's text, or Failed."""
    global busy, game
    if what == "show-folder":
        if game:
            open_path(game)
        return ""
    if what == "updates":
        return check_updates()
    with status_lock:
        if busy:  # a message is still up, or the page is open twice
            if what == "mod":
                raise Failed("busy")
            return ("on" if game and os.path.exists(os.path.join(game, "wasdmod-record.flag")) else "off") if what == "record" \
                else "" if what == "find-game" else "cancelled"
        busy = True
    status_changed()
    try:
        if what == "choose-folder":
            return choose_folder()
        if what == "find-game":
            remove(os.path.join(store_dir(), "game-folder"))
            game = find_game()
            return ""
        if what == "export":
            return export_file(arg, body)
        if not game:
            raise Failed(T(NOT_FOUND))
        if what == "record":
            run_record()
            return "on" if os.path.exists(os.path.join(game, "wasdmod-record.flag")) else "off"
        run = {"install": run_install, "toggle": run_toggle, "uninstall": run_uninstall}.get(arg)
        if not run:
            raise Failed("Unknown request.")
        failed = run()
        if failed:
            raise Failed(failed)
        return ""
    finally:
        busy = False
        status_changed()


# ---------------------------------------------------------------- the editor's server
#
# GET /<token>/ is the page; everything else is POST /<token>/<name>[?<argument>], as
# listed in the page's first script. Anything without the token is answered 404, before
# its body is read. A request's body is at most 2 MB.
MAXREQ = 2 << 20
token = secrets.token_hex(16)  # 32 hex digits, new at every start
# For ending when the editor is closed: the page's requests at work now, when the last one
# ended, whether there has been a page, and whether the page dropped a request that waited
# (which is what closing its window does).
active, last_seen, page_seen, dropped = 0, time.monotonic(), False, False
count_lock = threading.Lock()


def controls_path():
    """The game's own keyboard settings (Settings > Controls > Keyboard), inside the game's Proton prefix."""
    pfx = prefix_of(game) if game else None
    return os.path.join(pfx, "drive_c/users/steamuser/AppData/Local/Dungeons2/Saved/SaveGames/EnhancedInputUserSettings.sav") if pfx else None


def controls_stamp():
    """The file's stamp: its size and the time it was written ("" when there's no file)."""
    try:
        st = os.stat(controls_path() or "")
        return "%x-%x" % (st.st_size, st.st_mtime_ns)
    except OSError:
        return ""


def controls_write(body):
    """The game's keyboard settings from the editor (base64): (status, text). The first write keeps the original."""
    path = controls_path()
    if not path or not os.path.isfile(path):
        return 500, T("The in-game controls file wasn't found. Change any key in the game's settings once, then try again.")
    if game_running():
        return 500, error_text(ERR_RUNNING)
    try:
        data = base64.b64decode(body)
    except ValueError:
        data = b""
    if len(data) < 8 or data[:4] != b"GVAS":
        return 500, "Not a settings file."
    try:
        if not os.path.exists(path + ".wasdmod-backup"):
            shutil.copy2(path, path + ".wasdmod-backup")
    except OSError:
        return 500, T("Couldn't write the in-game controls file.")
    if not write_file(path, data):
        return 500, T("Couldn't write the in-game controls file.")
    return 200, "ok"


def query_name(query):
    """"name=..." in a request's address, as text."""
    m = re.match(r"name=([^&]*)", query)
    try:
        return re.sub(r"%([0-9A-Fa-f]{2})", lambda h: chr(int(h.group(1), 16)), m.group(1).replace("+", " ")).encode("latin-1").decode("utf-8", "replace") if m else ""
    except ValueError:
        return ""


def save_layout(name, body):
    """A layout from the editor, into the game folder: (status, text) as the apps answer."""
    low = name.lower()
    keep = low in ("author.txt", "default.txt") or (len(name) > 11 and low.startswith("wasdmod") and low.endswith(".txt"))
    if not keep or re.search(r'[\\/:\x00-\x1f]', name):  # only the editor's names; anything else is wasdmod.txt
        name = "wasdmod.txt"
    elif low in ("author.txt", "default.txt"):
        name = low
    if not game or not is_game_dir(game):
        return 409, T(NOT_FOUND)
    if b"[Buttons]" not in body and b"[Move]" not in body:
        return 400, T("That isn't a key layout.")
    if not put_layout(game, name, body):
        return 500, error_text(ERR_WRITE)
    state = install_state(game)
    status_changed()
    # Saved but not in use yet: "saved:" tells the editor it isn't an error.
    if state == TURNED_OFF:
        return 409, "saved:" + T("Saved, but wasdmod is disabled: click Enable at the top.")
    if state not in (INSTALLED_LATEST, INSTALLED_OLDER):
        return 409, "saved:" + T("Saved, but the mod isn't installed yet: click Install Mod at the top.")
    return 200, os.path.basename(active_settings(game))


def page():
    """The editor with what makes its window.wasdmodHost, after the page's first four lines: the saved
    layouts and this script's settings as two JSON script elements (see the page's first script)."""
    html = payload("Key Layout Editor.html")
    at = 0
    for _ in range(4):
        at = html.find(b"\n", at) + 1
    saved = read_bytes(os.path.join(store_dir(), "editor.json"), MAXREQ) or b""
    if not saved.startswith(b"{") or b"</" in saved:
        saved = b"{}"
    in_use = None  # the layout the game uses, so the editor can show it
    if game and install_state(game) != NOT_INSTALLED:
        path = active_settings(game)
        text = read_bytes(path, MAXREQ)
        if text is not None:
            in_use = {"file": os.path.basename(path), "text": text.decode("utf-8", "replace")}
    with status_lock:
        status, seq = status_now, status_seq
    host = '{"base":"/%s/","app":"linux","version":%s,"updates":%s,"lang":%s,"game":%s,"without":%s,"status":%s,"seq":%d}' % (
        token, to_json(VERSION), to_json(UPDATES), to_json(LANG), to_json(in_use), to_json([] if dialog_tool() else ["exportFile"]), status, seq)
    return (html[:at] + b'<script type="application/json" id="wasdmod-store">' + saved + b'</script>\n'
            + b'<script type="application/json" id="wasdmod-host">' + host.encode("utf-8") + b'</script>\n' + html[at:])


class Handler(http.server.BaseHTTPRequestHandler):
    timeout = 15  # (a connection that doesn't send its request within 15 seconds is closed)

    def log_message(self, *args):
        pass

    def reply(self, code, body, kind="text/plain; charset=utf-8"):
        global dropped
        if isinstance(body, str):
            body = body.encode("utf-8")
        try:
            self.send_response(code)
            self.send_header("Content-Type", kind)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except OSError:
            dropped = True  # the page is gone

    def gone(self):
        """The page closed this request (its window was closed)."""
        global dropped
        try:
            closed = bool(select.select([self.connection], [], [], 0)[0]) and self.connection.recv(1, socket.MSG_PEEK) == b""
        except OSError:
            closed = True
        if closed:
            dropped = True
        return closed

    def ours(self):
        """The address starts with /<token>/."""
        return secrets.compare_digest(self.path[:34].encode("latin-1", "replace"), ("/%s/" % token).encode())

    def page_request(self, answer):
        """One of the page's requests, counted while it's at work."""
        global active, last_seen, dropped
        if not self.ours():
            return self.reply(404, "Not found.")
        with count_lock:
            active += 1
            dropped = False
        try:
            answer()
        except Failed as e:
            self.reply(500, str(e))
        except Exception as e:  # (a mistake here: the page gets it as the reason, instead of waiting for nothing)
            self.reply(500, "%s: %s" % (type(e).__name__, e))
        finally:
            with count_lock:
                active -= 1
                last_seen = time.monotonic()

    def do_GET(self):
        self.page_request(self.get)

    def do_POST(self):
        self.page_request(self.post)

    def do_other(self):
        self.reply(404, "Not found.")

    do_HEAD = do_PUT = do_DELETE = do_OPTIONS = do_PATCH = do_other

    def get(self):
        global page_seen
        if self.path != "/%s/" % token:
            return self.reply(404, "Not found.")
        status_changed()  # (the page starts with the state as it is now)
        page_seen = True
        self.reply(200, page(), "text/html; charset=utf-8")

    def post(self):
        global LANG
        name, _, arg = self.path[len(token) + 2:].partition("?")
        try:
            size = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            size = -1
        if not 0 <= size <= MAXREQ:
            return self.reply(413, "Too large.")
        body = self.rfile.read(size)
        if name == "store":
            ok = body.startswith(b"{") and keep("editor.json", body)
            self.reply(200 if ok else 500, "" if ok else "Couldn't keep the layouts.")
        elif name == "lang":
            code = lang_of(arg)
            if code:
                LANG = code
                status_changed()
            self.reply(200, "")
        elif name == "save":
            self.reply(*save_layout(query_name(arg), body))
        elif name == "controls":
            data = read_bytes(controls_path() or "", MAXREQ)
            self.reply(200, base64.b64encode(data) if data else b"")
        elif name == "controls-wait":
            until = time.monotonic() + 25
            while controls_stamp() == arg and time.monotonic() < until and not self.gone():
                time.sleep(0.5)  # (looked at twice a second while a page waits; the game writes the file in a few steps)
            self.reply(200, controls_stamp())
        elif name == "controls-write":
            self.reply(*controls_write(body))
        elif name == "status-wait":
            status_changed()  # (what happened outside, like the game being closed, is seen here)
            until = time.monotonic() + 25
            with status_lock:
                while str(status_seq) == arg and time.monotonic() < until and not self.gone():
                    status_lock.wait(1)
                answer = '{"seq":%d,"status":%s}' % (status_seq, status_now)
            self.reply(200, answer, "application/json; charset=utf-8")
        elif name in ("mod", "choose-folder", "show-folder", "find-game", "record", "export", "updates"):
            self.reply(200, page_call(name, query_name(arg) if name == "export" else arg, body))
        else:
            self.reply(404, "Not found.")


def open_editor(url):
    """The editor as an app window (Chrome, Chromium, Brave or Edge), otherwise in the default browser."""
    for name in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "brave-browser", "microsoft-edge"):
        if shutil.which(name):
            subprocess.Popen([name, "--app=" + url, "--window-size=1200,860"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return
    open_path(url)


def serve(open_window):
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)  # this computer only; port 0: any free one
    server.daemon_threads = True
    url = "http://127.0.0.1:%d/%s/" % (server.server_address[1], token)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    status_changed()
    if not open_window:
        tell(url)
    else:
        tell(T("wasdmod is running: the key layout editor is open in your browser."))
        tell(T("To quit, close the editor's window, or press Ctrl+C here."))
        tell(url)
        open_editor(url)
#if !NEXUS
        threading.Thread(target=daily_check, daemon=True).start()
#endif
    try:
        # Until the editor is closed: the page dropped a request that waited (an open page always has
        # one waiting, status-wait) and has asked nothing for 10 seconds since. A page that only went
        # quiet may be asleep in a browser tab: the script stays for it (half a day at most).
        while True:
            time.sleep(1)
            with count_lock:
                idle = 0 if active else time.monotonic() - last_seen
            if (dropped and idle > 10) or idle > 12 * 3600:
                break
    except KeyboardInterrupt:
        pass
    server.shutdown()
    server.server_close()


# ---------------------------------------------------------------- entry

def main(argv):
    global game
    try:
        sys.stdout.reconfigure(errors="replace")  # (a terminal that can't show a language's letters)
    except (AttributeError, ValueError):
        pass
    commands = ("status", "install", "uninstall", "on", "off", "find")
    args = [a for a in argv if a != "--no-open"]
    if any(a in ("-h", "--help") for a in args):
        print(__doc__.strip())
        return 0
    command = args.pop(0) if args and args[0] in commands else None
    if args:  # the game's folder or exe, after the command
        game = dir_from_pick(args[0])
        if not game:
            print("Game not found there; pass its folder or executable.")
            return 1
    else:
        kept = (read_bytes(os.path.join(store_dir(), "game-folder"), 4096) or b"").decode("utf-8", "surrogateescape").strip()
        game = kept if kept and is_game_dir(kept) else find_game()  # one picked by hand wins while it exists
    if not command:
        if not payload("Key Layout Editor.html") or not payload("xinput1_4.dll"):
            print("Key Layout Editor.html and xinput1_4.dll must be in the folder this script is in: unzip the whole wasdmod folder.")
            return 1
        pick_language()
        serve("--no-open" not in argv)
        return 0
    if not game:
        print("not found" if command == "find" else "Game not found; pass its folder or executable.")
        return 1
    if command == "find":
        print(game)
        return 0
    if command == "status":
        state = install_state(game)
        line = STATE_NAMES[state]
        if state in (INSTALLED_LATEST, INSTALLED_OLDER, TURNED_OFF):
            line += ", key layout: " + os.path.basename(active_settings(game))
            line += ", Proton: " + ("loads it" if override_ok(game) else "needs the launch option " + (LAUNCH_OPTION if in_steam(game) else ENV_OPTION))
        print(line)
        return state
    if command == "install":
        code = install(game)
        print(error_text(code) if code else "Installed." if override_ok(game)
              else "Installed. Proton still loads its own xinput1_4.dll: paste this into the game's Launch Options in Steam: " + LAUNCH_OPTION if in_steam(game)
              else "Installed. For the game to load it, add this environment variable in your launcher's settings for the game: " + ENV_OPTION)
    elif command == "uninstall":
        code = uninstall(game)
        print(error_text(code) if code else "Uninstalled.")
    else:
        on = command == "on"
        code = 1 if install_state(game) == NOT_INSTALLED else turn_on(game, on)
        print("Couldn't switch it (not installed, or the file is in use)." if code else "Turned on." if on else "Turned off.")
    return code


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
