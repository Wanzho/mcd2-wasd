#!/usr/bin/env python3
"""Checks a built disk image without opening Finder, and can draw a preview.

  python check.py build/wasdmod-Mac.dmg [--preview out.png]

Mounts the image read-only and hidden, reads its window layout (.DS_Store) and
checks that: the window shows a picture background whose alias points to the
two-size .background.tiff on this volume; the toolbar, sidebar, path and status
bars are off; every visible item has a position inside the window; the volume
has its custom icon; the app inside is validly signed. With --preview it draws
the window (preview.swift: the real background, icons and names at the stored
positions) as a PNG. Run it with the virtualenv make-dmg.sh makes (it has
ds_store and mac_alias).
"""
import json, os, re, subprocess, sys, tempfile

from ds_store import DSStore
from mac_alias import Alias

TITLE_BAR = 32  # macOS 26, a window without a toolbar


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def main():
    args = sys.argv[1:]
    preview = None
    if "--preview" in args:
        i = args.index("--preview")
        preview = os.path.abspath(args[i + 1])
        del args[i:i + 2]
    dmg = args[0]
    here = os.path.dirname(os.path.abspath(__file__))
    problems = []

    work = tempfile.mkdtemp(prefix="wasdmod-check.")
    mnt = os.path.join(work, "mnt")
    os.mkdir(mnt)
    run("hdiutil", "attach", "-quiet", "-nobrowse", "-readonly", "-noautoopen", "-mountpoint", mnt, dmg)
    try:
        volname = run("diskutil", "info", mnt)
        volname = re.search(r"Volume Name:\s*(.+)", volname).group(1).strip()
        with DSStore.open(os.path.join(mnt, ".DS_Store"), "r") as d:
            bwsp = d["."]["bwsp"]
            icvp = d["."]["icvp"]
            locations = {}
            for e in d:
                if e.code == b"Iloc":
                    locations[e.filename] = e.value

        # window
        m = re.match(r"\{\{(-?\d+), (-?\d+)\}, \{(\d+), (\d+)\}\}", bwsp["WindowBounds"])
        x, y, w, h = map(int, m.groups())
        content = (w, h - TITLE_BAR)
        print(f"volume {volname!r}, window frame {w} x {h} at ({x}, {y}): {content[0]} x {content[1]} points of picture")
        for key in ("ShowToolbar", "ShowSidebar", "ShowStatusBar", "ShowPathbar", "ShowTabView"):
            if bwsp.get(key):
                problems.append(f"{key} is on")
        print(f"icon size {icvp['iconSize']:g}, text size {icvp['textSize']:g}, arranged by {icvp['arrangeBy']}")

        # background
        bg = None
        if icvp.get("backgroundType") != 2:
            problems.append("the window has no background picture")
        else:
            alias = Alias.from_bytes(icvp["backgroundImageAlias"])
            target = alias.target.filename
            print(f"background alias: {target!r} on volume {alias.volume.name!r}")
            bg = os.path.join(mnt, target)
            if alias.volume.name != volname:
                problems.append(f"the background alias names volume {alias.volume.name!r}")
            if not os.path.isfile(bg):
                problems.append(f"the background {target} isn't on the volume")
            else:
                sizes = re.findall(r"Image Width: (\d+) Image Length: (\d+)", run("tiffutil", "-info", bg))
                print("background sizes:", ", ".join("x".join(s) for s in sizes))
                if len(sizes) < 2:
                    problems.append("the background has no Retina (2x) version")
                elif int(sizes[0][0]) < content[0] or int(sizes[0][1]) < content[1] + 4:
                    problems.append("the background is smaller than the window")

        # items
        visible = sorted(n for n in os.listdir(mnt) if not n.startswith("."))
        items = []
        for name in visible:
            if name not in locations:
                problems.append(f"{name} has no position")
                continue
            ix, iy = locations[name]
            half = icvp["iconSize"] / 2
            inside = half <= ix <= content[0] - half and half <= iy <= content[1] - half - icvp["textSize"] * 1.6
            print(f"  {name!r} at ({ix}, {iy}){'' if inside else '  <- not fully inside the window'}")
            if not inside:
                problems.append(f"{name} isn't fully inside the window")
            path = os.path.join(mnt, name)
            label = name
            if name.endswith(".app"):
                label = name[:-4]
            elif "." in name and run("GetFileInfo", "-aE", path).strip() == "1":
                label = name.rsplit(".", 1)[0]
            items.append({"path": path, "label": label, "x": ix, "y": iy})
        if "Applications" not in visible or os.readlink(os.path.join(mnt, "Applications")) != "/Applications":
            problems.append("no Applications link")

        # volume icon (the custom-icon flag in the root's Finder info)
        info = run("xattr", "-px", "com.apple.FinderInfo", mnt) if os.path.exists(os.path.join(mnt, ".VolumeIcon.icns")) else ""
        flags = bytes.fromhex(info.replace(" ", "").replace("\n", ""))[8:10] if info else b"\0\0"
        if not int.from_bytes(flags, "big") & 0x0400:
            problems.append("the volume has no custom icon")
        else:
            print("volume icon: set")

        # signature
        for name in visible:
            if name.endswith(".app"):
                r = subprocess.run(["codesign", "--verify", "--deep", "--strict", os.path.join(mnt, name)], capture_output=True, text=True)
                print(f"signature of {name}: {'valid' if r.returncode == 0 else r.stderr.strip()}")
                if r.returncode:
                    problems.append(f"{name}'s signature is broken")

        if preview and bg:
            spec = {"background": bg, "width": content[0], "height": content[1], "iconSize": icvp["iconSize"],
                    "textSize": icvp["textSize"], "title": volname, "appearance": "light", "items": items, "out": preview}
            spec_path = os.path.join(work, "spec.json")
            with open(spec_path, "w") as f:
                json.dump(spec, f)
            subprocess.run(["swift", os.path.join(here, "preview.swift"), spec_path], check=True)
    finally:
        subprocess.run(["hdiutil", "detach", "-quiet", mnt], check=False)
        try:
            os.rmdir(mnt)
            os.rmdir(work)
        except OSError:
            pass

    if problems:
        print("PROBLEMS:\n  " + "\n  ".join(problems))
        sys.exit(1)
    print("OK")


main()
