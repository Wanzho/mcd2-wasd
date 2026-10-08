# dmgbuild settings for the Mac disk image; make-dmg.sh passes the paths with -D.
# The window shows 660 x 420 points of background.png; the icon positions come
# from layout.js, which background.html draws around.
import json
import os.path

app = defines["app"]
readme = defines.get("readme") or None
with open(defines["layout"], encoding="utf-8") as f:
    layout = json.loads(f.read().split("const LAYOUT =", 1)[1].strip().rstrip(";"))

format = "UDZO"
compression_level = 9
filesystem = "HFS+"

files = [app] + ([readme] if readme else [])
symlinks = {"Applications": "/Applications"}
hide_extensions = [os.path.basename(readme)] if readme else []  # "Read me", not "Read me.txt"

icon = defines["icon"]              # the volume's icon: the app's
background = defines["background"]  # background.png; dmgbuild adds background@2x.png (one HiDPI TIFF)

# A plain icon window: no toolbar, sidebar, path or status bar.
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
default_view = "icon-view"
show_icon_preview = False
arrange_by = None

# Finder stores the whole window frame, title bar included: 32 points of title bar
# on macOS 26 leave exactly 420 for the picture (28 on macOS 11-15: 424, and the
# picture is taller than that). The position is from the bottom left of the screen;
# Finder keeps the window on screen.
window_rect = ((400, 320), (660, 452))

icon_size = layout["iconSize"]
text_size = layout["textSize"]
names = {"app": os.path.basename(app.rstrip("/")), "applications": "Applications",
         "readme": os.path.basename(readme) if readme else None}
icon_locations = {names[i["role"]]: (i["x"], i["y"]) for i in layout["items"] if names[i["role"]]}
