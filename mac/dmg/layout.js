// The disk image window's layout, shared by background.html (which draws the
// picture around it) and settings.py (which places the icons; it reads the JSON
// after "const LAYOUT"). Points from the top left of the picture; x, y is the
// centre of an icon; "label" is the name Finder shows under it.
const LAYOUT = {
  "iconSize": 112,
  "textSize": 13,
  "items": [
    {"role": "app", "label": "wasdmod", "x": 168, "y": 186, "cap": "warm"},
    {"role": "applications", "label": "Applications", "x": 492, "y": 186, "cap": "cool"},
    {"role": "readme", "label": "Read me", "x": 584, "y": 320, "cap": ""}
  ],
  "stepsTop": 302
};
