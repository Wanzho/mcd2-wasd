#!/usr/bin/env python3
"""Builds the translations into each part of wasdmod (run by build.sh).

lang/<code>.json has one language: its name, and for each part of wasdmod the
English text with its translation:
  "game"    the on-screen key list and typing banner (xinput_wasd.c, legend.inc)
  "setup"   the Windows setup (installer.c, updater.inc)
  "mac"     the Mac app (mac/main.swift, mac/updater.swift, and the messages of mac/wasdmod.sh)
  "editor"  the key layout editor (configurator.html)
English is the key, so text without a translation shows in English. Text that
changes with a number is {"one": ..., "other": ...} (with "few"/"many" where
the language has them), keyed by the English "other" form.

Writes:
  build/lang_game.h    the "game" text as C tables
  build/lang_setup.h   the "setup" text as C tables
  build/lang.json      the "mac" text, for the Mac app
  Keybinder.html       configurator.html with every language's "editor" text built in
                       (WASDMOD_FLAVOR=nexus: without the web fonts it loads as a plain page)

python3 lang.py --check   lists, per language, the text the code uses that it
                          doesn't translate yet, and translations nothing uses.
"""
import json, os, re, sys

# The game's languages; one Portuguese and one Spanish, French and English.
ORDER = ["en", "de", "es", "fr", "it", "nl", "pl", "pt", "sv", "tr", "ru", "uk", "ja", "ko", "zh-Hans", "zh-Hant"]
PARTS = ["game", "setup", "mac", "editor"]


def load():
    langs = {"en": {"name": "English", **{p: {} for p in PARTS}}}
    found = sorted(f[:-5] for f in os.listdir("lang") if f.endswith(".json"))
    for code in found:
        with open(os.path.join("lang", code + ".json"), encoding="utf-8") as f:
            langs[code] = json.load(f)
    order = [c for c in ORDER if c in langs] + [c for c in found if c not in ORDER]
    return {c: langs[c] for c in order}


# ---------------------------------------------------------------- text the code uses

STR = r'"((?:[^"\\\n]|\\.)*)"'


def unescape(s):
    return json.loads('"' + s + '"')


NEXUS = os.environ.get("WASDMOD_FLAVOR") == "nexus"


def flavor(src, nexus):
    """The source as the given build compiles it: #ifndef NEXUS / #ifdef NEXUS (C) and
    #if !NEXUS / #if NEXUS (Swift) blocks kept or dropped, like the compiler does."""
    out, stack = [], []  # stack: True/False for a NEXUS block (kept or not), None for any other #if
    for line in src.split("\n"):
        t = line.strip()
        m = re.match(r"#\s*(ifndef|ifdef|if)\s+(!?)\s*NEXUS\b\s*$", t)
        if m:
            stack.append(nexus == (m.group(1) == "ifdef" or (m.group(1) == "if" and not m.group(2))))
            continue
        if re.match(r"#\s*if", t):
            stack.append(None)
        elif re.match(r"#\s*else\b", t) and stack and stack[-1] is not None:
            stack[-1] = not stack[-1]
            continue
        elif re.match(r"#\s*endif\b", t) and stack:
            if stack.pop() is not None:
                continue
        if all(k is not False for k in stack):
            out.append(line)
    return "\n".join(out)


def used(nexus=None):
    """English text each part uses: {part: {text: plural?}}. With nexus=True/False,
    only what that build compiles (the Nexus Mods build has no updater); by default both."""
    out = {p: {} for p in PARTS}
    def scan(part, path, patterns):
        src = open(path, encoding="utf-8").read()
        if nexus is not None:
            src = flavor(src, nexus)
        for pat, group, plural in patterns:
            for m in re.finditer(pat, src):
                raw = m.group(group)
                out[part][raw if plural is None else unescape(raw)] = bool(plural)
    c = [(r'\bT\(\s*' + STR, 1, False), (r'\bN_\(\s*' + STR, 1, False)]
    scan("game", "xinput_wasd.c", c)
    scan("game", "legend.inc", c)
    scan("setup", "installer.c", c)
    scan("setup", "updater.inc", c)
    scan("mac", "mac/main.swift", [(r'\bL\(\s*' + STR, 1, False)])
    scan("mac", "mac/updater.swift", [(r'\bL\(\s*' + STR, 1, False)])
    for m in re.finditer(r'\bfail "([^"$]*)"', open("mac/wasdmod.sh", encoding="utf-8").read()):
        if " " in m.group(1) and not m.group(1).startswith(("usage", "use ", "record ")):
            out["mac"][m.group(1)] = False
    scan("editor", "configurator.html", [
        (r'\bt\(\s*' + STR, 1, False), (r'\btx\(\s*' + STR, 1, False), (r'\b_\(\s*' + STR, 1, False),
        (r'\btn\(\s*' + STR + r'\s*,\s*' + STR, 2, True),
        (r'data-t(?:x|-title|-aria|-placeholder)?="([^"]*)"', 1, None)])
    return out


def check(langs):
    need = used()
    bad = 0
    for code, l in langs.items():
        if code == "en":
            continue
        for part in PARTS:
            have = l.get(part, {})
            missing = [k for k in need[part] if not have.get(k)]
            unused = [k for k in have if k not in need[part]]
            wrong = [k for k, plural in need[part].items() if plural and k in have and not isinstance(have[k], (dict, str))]
            for k in missing:
                print("%s %s: missing %s" % (code, part, json.dumps(k, ensure_ascii=False)))
            for k in unused:
                print("%s %s: unused %s" % (code, part, json.dumps(k, ensure_ascii=False)))
            for k in wrong:
                print("%s %s: not text or plural forms %s" % (code, part, json.dumps(k, ensure_ascii=False)))
            bad += len(missing) + len(wrong)
    print("%d language(s), %s" % (len(langs), "everything translated" if not bad else "%d missing" % bad))
    return bad


# ---------------------------------------------------------------- output

def c_string(s):
    out = []
    for b in s.encode("utf-8"):
        c = chr(b)
        if c == '"' or c == "\\":
            out.append("\\" + c)
        elif c == "\n":
            out.append("\\n")
        elif 32 <= b < 127:
            out.append(c)
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def c_tables(langs, part, path, only=None):
    keys = []
    for l in langs.values():
        for k in l.get(part, {}):
            if k not in keys and isinstance(l[part][k], str) and (only is None or k in only):
                keys.append(k)
    lines = ["// Generated by lang.py from lang/*.json (\"%s\"); do not edit." % part,
             "#define LANG_COUNT %d" % len(langs),
             "static const char *const LANG_CODE[LANG_COUNT] = {%s};" % ", ".join(c_string(c) for c in langs),
             "static const char *const LANG_NAME[LANG_COUNT] = {%s};" % ", ".join(c_string(l["name"]) for l in langs.values()),
             "#define TR_COUNT %d" % max(len(keys), 1),
             "static const char *const TR_KEY[TR_COUNT] = {%s};" % (", ".join(c_string(k) for k in keys) or "0"),
             "static const char *const TR_TEXT[LANG_COUNT][TR_COUNT] = {"]
    for code, l in langs.items():
        have = l.get(part, {})
        row = [c_string(have[k]) if isinstance(have.get(k), str) and have[k] and code != "en" else "0" for k in keys] or ["0"]
        lines.append("    {%s}, // %s" % (", ".join(row), code))
    lines.append("};")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    langs = load()
    if "--check" in sys.argv:
        sys.exit(1 if check(langs) else 0)
    os.makedirs("build", exist_ok=True)
    # The Nexus Mods build gets only the text it uses (none of the updater's).
    keep = used(nexus=True) if NEXUS else None
    c_tables(langs, "game", "build/lang_game.h")
    c_tables(langs, "setup", "build/lang_setup.h", keep and keep["setup"])
    with open("build/lang.json", "w", encoding="utf-8") as f:
        json.dump({"order": list(langs), "names": {c: l["name"] for c, l in langs.items()},
                   "text": {c: {k: v for k, v in l.get("mac", {}).items() if keep is None or k in keep["mac"]}
                            for c, l in langs.items() if c != "en"}}, f, ensure_ascii=False)
    # The key layout editor: its languages in place of the English-only line, and the
    # doctype a local file needs (the published page gets its wrapper from the host).
    page = open("configurator.html", encoding="utf-8").read()
    built = {c: {"name": l["name"], "t": l.get("editor", {}) if c != "en" else {}} for c, l in langs.items()}
    line = re.search(r"^const LANGS = .*$", page, re.M)
    assert line, "configurator.html has no LANGS line"
    js = json.dumps(built, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    page = page[:line.start()] + "const LANGS = " + js + ";" + page[line.end():]
    # The Nexus Mods build has no internet code at all: opened as a plain page (outside
    # the apps), its editor uses the system's fonts instead of loading Chivo from Google.
    if NEXUS:
        page, n = re.subn(r'\nelse document\.head\.append\(Object\.assign\(document\.createElement\("link"\), \{ rel: "stylesheet",\s*'
                          r'href: "https://fonts\.googleapis\.com/[^"]*" \}\)\);', "\n// (Nexus Mods build: no web fonts.)", page)
        assert n == 1 and "googleapis" not in page, "configurator.html: the web font line changed"
    with open("Keybinder.html", "w", encoding="utf-8") as f:
        f.write('<!doctype html>\n<html lang="en">\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width,initial-scale=1">\n' + page)
    need, gaps = used(), 0
    for code, l in langs.items():
        if code != "en":
            gaps += sum(1 for part in PARTS for k in need[part] if not l.get(part, {}).get(k))
    print("Languages: %s%s" % (", ".join(langs), "" if not gaps else " (%d texts not translated yet: python3 lang.py --check)" % gaps))


if __name__ == "__main__":
    main()
