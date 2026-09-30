# Minecraft Dungeons II Controller Mod

Minecraft Dungeons II on PC moves by clicking. This mod lets you play with
WASD, the mouse and the keyboard instead, by turning them into a virtual
controller. The game already supports controllers, so it plays exactly like a
pad: smooth movement, dodges, artifacts, menus, with the keyboard still
available for menus and typing.

It is one file, `xinput1_4.dll`, placed next to the game's exe, plus a settings
file. The game itself is not modified. Tested on a Mac through CrossOver;
the Windows installer is built and checked but not yet tested on a Windows PC.

## Default controls

| Key | Does |
|---|---|
| W A S D | Move |
| Space or F | Jump / interact |
| Left click | Melee |
| Right click | Dodge the way you're moving; press, drag 40 px and release to roll that way |
| Shift | Forward dodge |
| Side button 5 | Bow (hold it to aim with the mouse) |
| Q / 2 / 3 | Artifacts 1 / 2 / 3 |
| R | Health potion |
| Middle click | Guidance trail |
| E, I or ↑ | Inventory (tap: full inventory, hold: mini inventory) |
| ↓ | Social menu |
| ← or X | Teleport to player |
| → or V | Track quest / quest log |
| Esc or Tab | Game menu |
| B | Collectibles |
| M, J, U, K, G | The game's own menu shortcuts |
| T | Typing mode (every key goes to the game until Esc) |
| Alt (Option on a Mac) or Ctrl | Hold for the mouse cursor |
| F9 | Show or hide the on-screen key list |
| Backtick (`) | Turn the mod off and on |

Opening a menu switches to **mouse mode**: the cursor and keyboard work
normally, and clicks never leave it. Any fight key (WASD, Space, Shift...)
switches back. Holding ⌘ (the Windows key on a PC) blocks every key, so system
shortcuts never fire an ability.

## Install

**Windows:** run `dist/Dungeons II Controller Mod Setup.exe`. It finds the game
through Steam (or use Browse), installs, updates and uninstalls, and opens the
key layout editor. The file isn't signed, so Windows may say "Windows protected
your PC": click More info, then Run anyway. Command line: `/find`, `/status`,
`/install`, `/uninstall` (optionally followed by the game's Win64 folder).

**Mac (CrossOver):**

```sh
./build.sh           # Apple clang + the lld-link from a Rust toolchain
./install.command    # default bottle: "Steam"; pass another bottle folder as an argument
```

The Mac installer also sets three CrossOver options for the bottle: prefer the
game folder's `xinput1_4.dll` for this game (Wine uses its own otherwise), send
Option as Alt, and confine the cursor to a screen area rather than one window.
`./uninstall.command` undoes all of it.

**Linux / Steam Deck (Proton):** copy `xinput1_4.dll` and `wasd-mod.ini` into
`Dungeons/Binaries/Win64` and set the launch option
`WINEDLLOVERRIDES=xinput1_4=n,b %command%` (untested).

## Changing keys

`configurator.html` is the key layout editor (`Keybinder.html` is the offline
copy that build.sh generates). It saves a `wasd-mod.txt`/`wasd-mod.ini`; put it
in the game's Win64 folder and restart the game. The mod reads whichever of
`wasd-mod.ini` and `wasd-mod.txt` changed last, and the installers keep a saved
`wasd-mod.txt` in charge. You can also edit `wasd-mod.ini` directly; every
option is explained in it. A few worth knowing:

- `MouseMoveSwitches=0`: moving the mouse never leaves controller mode; only
  Alt, the menu keys or backtick give you the mouse.
- `CursorMode=Toggle`: Alt shows the cursor on one press and locks it on the next.
- `BowAimsWithMouse=1`: holding the bow's mouse button switches to keyboard mode
  so the game aims the bow at the cursor.
- `BumpNudgePct`: after a small mouse bump the game shows keyboard prompts; the
  mod flips it back with a tiny right-stick nudge. Lower it if you ever dodge by accident.

`wasd-mod.log` next to the DLL records what the mod does (mode switches, text
boxes, timing).

## How it works

The DLL stands in for XInput: it reports controller 0 built from the keyboard
and mouse, and forwards real controllers to the system's XInput. A message hook
on the game's window threads hides the mapped keys and mouse (including the
raw-input copies the game also reads), so the game sees only the controller and
doesn't flicker between keyboard and controller mode. Text boxes are detected
through Text Services (IMM as a fallback) and get the keyboard back.

Things that were tried and don't work: patching the game's `GetCursorPos`
import (the game quits 20-30 s later), a moving overlay window (lag under
CrossOver), and `ClipCursor` (confined the mouse to the wrong area).

## Build and test

`./build.sh` builds `build/xinput1_4.dll`, `build/test_load.exe`, `Keybinder.html`
and the Windows installer (copied to `dist/`). No Windows SDK or C runtime is
needed. `build/test_load.exe` runs inside a Wine bottle and checks the exports
and the input filtering (gameplay, typing, menus, remaps, bow, text boxes);
`smooth_test.c` and `dodge_test.c` test the movement smoothing and drag dodge on
the host.
