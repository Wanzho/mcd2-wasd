
# wasdmod: WASD controls for Minecraft Dungeons II

Play Minecraft Dungeons II (Minecraft Dungeons 2) with WASD, mouse and keyboard instead of click-to-move, on Windows, Mac (CrossOver) and Linux.

wasdmod turns your keys into a virtual controller, so you get everything the game offers on a controller: smooth movement, dodges, artifacts and the menu wheel. Menus, the cursor and chat still work with the mouse and keyboard, and the game itself isn't modified.

It comes in the same 16 languages as the game: English, Deutsch, Español, Français, Italiano, Nederlands, Polski, Português (Brasil), Svenska, Türkçe, Русский, Українська, 日本語, 한국어, 简体中文 and 繁體中文 (see [Languages](#languages)).

<img width="1195" height="842" alt="Screenshot 2026-10-07 at 09 20 48" src="https://github.com/user-attachments/assets/1981122e-0cc5-4fd2-8cdc-e3d5f431381d" />

**Download:** [Windows](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.exe) · [Mac](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.dmg) · [Linux / Steam Deck](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.zip) · [all versions](https://github.com/Wanzho/mcd2-wasd/releases)

**Website:** [wasdmod.com](https://wasdmod.com/), with a playable demo and an interactive key map.

## Install

| System | Download |
|---|---|
| Windows | [**wasdmod-1.5.0.exe**](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.exe) |
| Mac (CrossOver) | [**wasdmod-1.5.0.dmg**](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.dmg) |
| Linux / Steam Deck, or Windows without the app | [**wasdmod-1.5.0.zip**](https://github.com/Wanzho/mcd2-wasd/releases/download/v1.5.0/wasdmod-1.5.0.zip) |

wasdmod works with the Steam version of the game, and with the version from Minecraft.net (Minecraft Launcher) or the Xbox app. On a Mac or Linux, the Launcher version works only if you already have it running there; MCD2 Crossover sets up the Steam version.

### Windows

1. Close the game and run **wasdmod-1.5.0.exe**.
2. If Windows says "Windows protected your PC", click **More info → Run anyway**. (The file isn't signed; that costs money every year.)
3. wasdmod opens with the key layout editor as its window (titled "wasdmod for Minecraft Dungeons II"). It looks for the game in Steam, and in `XboxGames` for the Minecraft Launcher and Xbox app. If it doesn't find it, open the **Game folder** menu at the top, click **Choose game folder…** and pick the game's `Dungeons-Win64-Shipping.exe` (Steam) or `Dungeons-WinGDK-Shipping.exe` (Minecraft Launcher and Xbox app).
4. Click **Install Mod** at the top right of the window, then start the game. For another layout, pick **Official layout** or **Recommended** under Layout and click **Install/Apply Layout**.

The **Game folder** menu also has **Show in Explorer** and **Find game automatically**.

The window uses Microsoft's WebView2, which Windows 11 has built in and most Windows 10 PCs have too. On a PC without it, wasdmod opens its setup window instead, with **Install**, **Browse…**, the layout choices and **Edit key layout…**, which opens the editor in Edge, Chrome or your browser. `wasdmod-1.5.0.exe /setup` opens the setup window on any PC.

### Mac

The game has to run in CrossOver first. If it doesn't yet, set it up with [MCD2 Crossover](https://github.com/Wanzho/mcd2-crossover), which fixes Steam startup and Microsoft sign-in. Then:

1. Open **wasdmod-1.5.0.dmg** and drag **wasdmod** into **Applications**.
2. Open wasdmod. If macOS blocks it, approve it in **System Settings → Privacy & Security → Open Anyway**. Keep Gatekeeper enabled.
3. Quit the game, click **Install Mod** at the top right of the window (titled "wasdmod for Minecraft Dungeons II"), then start the game.

If wasdmod doesn't find the game (a Minecraft Launcher copy, or a copy in another folder or bottle), click **Choose Game Folder…** and pick the game's folder, its `Dungeons-Win64-Shipping.exe`, or the CrossOver bottle it's in. The **Game folder** menu at the top of the key layout editor has the same choice, plus **Show in Finder** and **Find game automatically**.

Install also sets three CrossOver options, for Dungeons II only, so other games in the bottle keep their own settings: Wine uses wasdmod's `xinput1_4.dll` instead of its own, **Option** is sent as Alt, and the cursor stays inside the game window. **Uninstall** removes them. Versions before 1.0 set the last two for the whole bottle, which could throw the mouse around in other games there, like CS2. Installing a newer version moves them to Dungeons II only.

### Linux / Steam Deck (untested)

The game must already run under Proton: start it once before you install. On a Steam Deck, switch to Desktop Mode. Then:

1. Unzip **wasdmod-1.5.0.zip** and keep the files of its `wasdmod` folder together.
2. Start **wasdmod-linux.sh**: double-click it in the file manager and choose to run it, or open a terminal in the folder and enter `./wasdmod-linux.sh`. It needs Python 3, which the Steam Deck and most Linux systems have.
3. The key layout editor opens in your browser (in a window of its own if you have Chrome or Chromium). It looks for the game in Steam's libraries, also on an SD card. If it doesn't find it, open the **Game folder** menu at the top, click **Choose game folder…** and pick the game's `Dungeons-Win64-Shipping.exe`.
4. Quit the game, click **Install Mod** at the top right, then start the game.

`wasdmod-linux.sh` starts `wasdmod.py`, a plain Python script that you can read. It gives the editor the same buttons as the apps: **Install Mod**, **Disable**, **Uninstall**, the **Game folder** menu, **Install/Apply Layout** and **Record logs**. It needs no root rights and installs nothing on the system. It runs while the editor is open and ends a few seconds after you close the editor's window (or with Ctrl+C in the terminal); start it again to change keys, disable the mod or uninstall it.

Proton uses its own `xinput1_4.dll` unless it's told to load the one in the game folder. **Install Mod** says so for this game only, with one line in the game's Proton prefix (`steamapps/compatdata/1912410/pfx/user.reg`; the file as it was is kept beside it as `user.reg.wasdmod-backup`), and **Uninstall** takes the line out again. If that can't be done safely (the game hasn't run yet, Wine is still running, or the file already says something about `xinput1_4`), nothing is changed there, and the line under the editor's buttons shows a launch option instead. In Steam, right-click the game → **Properties → Launch Options** and paste it: `WINEDLLOVERRIDES="xinput1_4=n,b" %command%`

Without a browser, the script works from a terminal: `./wasdmod-linux.sh install`, or `status`, `uninstall`, `off`, `on` or `find`, each with the game's folder after it if the game isn't found by itself.

To install by hand instead, without the script:

1. In Steam, right-click the game → **Manage → Browse local files**, and open `Dungeons/Binaries/Win64`.
2. Copy `xinput1_4.dll` and `default.txt` there (and `author.txt` for the Recommended layout).
3. Right-click the game → **Properties → Launch Options** and enter `WINEDLLOVERRIDES="xinput1_4=n,b" %command%`

If you play the Minecraft Launcher copy outside Steam (Lutris, Heroic, Bottles…), start the script with the game's folder after its name, or copy the files into its `Content/Dungeons/Binaries/Win64` folder. Instead of the launch option, add the environment variable `WINEDLLOVERRIDES` = `xinput1_4=n,b` in that launcher's settings for the game.

### Windows without the app

The zip has everything the app does, as a script you can read:

1. Unzip **wasdmod-1.5.0.zip** (right-click → **Extract All**) and keep the files of its `wasdmod` folder together.
2. Double-click **wasdmod-windows.cmd**. Windows asks first, because the file came from the internet and isn't signed: in "Open File – Security Warning" ("The publisher could not be verified"), click **Run**. (If you get "Windows protected your PC" instead, click **More info → Run anyway**.)
3. A window named "wasdmod" stays open while it runs, and the key layout editor opens in a window of its own (Edge or Chrome, otherwise your browser). It looks for the game in Steam and in `XboxGames`. If it doesn't find it, open the **Game folder** menu at the top, click **Choose game folder…** and pick the game's exe.
4. Quit the game, click **Install Mod** at the top right, then start the game.

`wasdmod-windows.cmd` starts `wasdmod.ps1` with the PowerShell that Windows comes with. It's a plain script that you can open in Notepad, with the same buttons in the editor as the app. It needs no administrator rights, writes the mod's files into the game folder and changes no Windows settings. It ends when you close the editor's window. `wasdmod-windows.cmd install` (or `status`, `uninstall`, `off`, `on`, `find`) works without the editor.

To install by hand instead, copy `xinput1_4.dll` and `default.txt` (and `author.txt` for the Recommended layout) next to the game's exe. For the Steam version, that's `Dungeons\Binaries\Win64` under the game's folder (right-click the game → **Manage → Browse local files**). For the Minecraft Launcher or Xbox app version, the folder is `C:\XboxGames\Minecraft Dungeons II\Content\Dungeons\Binaries\WinGDK` (or `XboxGames` on the drive you installed to; `Win64` in older copies), next to `Dungeons-WinGDK-Shipping.exe`.

## Updating

The Mac app and the Windows app look on GitHub for a new version when they start (at most once a day) and when you choose **wasdmod → Check for Updates…** (Mac) or **Check for updates** at the end of the window (Windows). When there is one, a card appears at the top of the window. Nothing is downloaded until you click **Update now**. The update then runs in two steps, and the card shows each of them:

1. First the app updates itself. It downloads the new version from this repository's releases, checks it against the release's `SHA256SUMS` file, replaces the old app and reopens. The download doesn't go through your browser, so macOS doesn't ask you to approve it in Privacy & Security, and Windows SmartScreen doesn't stop it. If the app's folder can't be changed, the new version goes to your Downloads folder instead (on a Mac it opens in Finder), and you install it by hand.
2. Then the new app updates the mod in the game folder and keeps your key layouts and settings. If the game is running, this step waits and finishes by itself when you quit the game.

Versions 1.3.1 and older open the new version in your browser instead, so the first update from them is done by hand. After that, updating takes one click.

The check is a single request to GitHub's public API. It sends nothing about you beyond what every web request carries (your IP address, and the app's version as its user agent), and GitHub's privacy statement applies to it. Without an internet connection, wasdmod works as usual.

The zip's scripts only tell you about a new version: when they start (at most once a day) and when you click **Check for updates** at the end of the editor, they ask GitHub the same way and offer to open the releases page. They download nothing. To update, download the new zip, start its script and click **Update Mod**.

The Nexus Mods downloads don't check for updates. They contain no internet code at all, because Nexus Mods doesn't allow it. In those apps, and in the editor that the zip's scripts open, **Get updates on Nexus Mods** opens the mod's [Nexus Mods page](https://www.nexusmods.com/minecraftdungeons2/mods/104) in your browser.

## Controls

Two layouts are built in. The **Official layout** is the game's own keyboard keys, played as a controller; only the menu wheel moves from S (a movement key here) to Tab. **Recommended** is the author's layout.

| Action | Official layout | Recommended |
|---|---|---|
| Move | W A S D | W A S D |
| Jump / interact | Space | Space or F |
| Melee (in the air: heavy jump attack) | Left click | Left click |
| Bow (hold it to aim with the mouse) | Right click | Side button 4 (back) or C (C aims where you face) |
| Directional dodge (hold and drag the mouse to roll that way) | R or side button 4 | Right click |
| Forward dodge | Side button 5 | Left Shift |
| Artifacts 1 / 2 / 3 | 1 / 2 / 3 | Q / 2 / 3 |
| Health potion | E | R |
| Guidance trail | Middle click | Middle click |
| Inventory | I | E |
| World map | M | M |
| Menu wheel | Tab | Tab |
| Quests | J | V |
| Social menu | F | / |
| Emotes | G | G |
| Collectibles | U | B |
| Event log | K | K |
| Teleport to player | X, F1–F4 | X, F1–F4 |
| Teleport statue | Z | Z |

The menu wheel is on Tab because the game's own key for it, S, is "move down" here. Pressing Tab sends the game its S. To make keyboard mode match, set **Menu Wheel** to Tab in the game (Settings → Controls → Keyboard), or let the editor change it for you (see [Change keys](#change-keys)).

In both layouts:

| Key | Does |
|---|---|
| Esc | Game menu |
| T | Typing mode: every key goes to the game until Esc; a banner and an amber frame around the game show while it's on |
| Left Alt (left Option on a Mac) | Hold for the mouse cursor |
| F9 | Show or hide the on-screen key list (on a Mac keyboard: fn + F9, unless the F-keys are set as standard function keys) |
| Backtick (`) | Turn wasdmod off and on |

The banners at the top of the game have an **×** that closes them. A closed typing banner stays away until you start typing again, and the amber frame stays so you can still see you're typing. A closed "wasdmod off" banner stays away until you next turn wasdmod off, and a closed "Key layout loaded" banner until the next layout loads. To leave out the typing banner or the "wasdmod off" banner for good, turn off **Show the typing banner** or **Show the "wasdmod off" banner** under **Mod keys** in the editor.

Opening a menu or moving the mouse switches to **mouse mode**, where the cursor and keyboard work normally. Any fight key (WASD, Space…) switches back to the controller. Holding ⌘ (the Windows key on a PC) blocks every key, so system shortcuts never fire an ability.

## Change keys

The key layout editor is the main window of the Mac app and of the Windows app, and what the zip's scripts open in your browser.

- Pick **Official layout** or **Recommended** under Layout, or one of your own. Click a key to change it, or drag it onto another key on the keyboard map to move what it does (if both keys do something, they swap). The ⓘ next to a name says what a row or an option does, and the one next to a section's heading what the section is for.
- Click **Install/Apply Layout**. A running game switches to the layout within a second and shows a "Key layout loaded" banner with its name for a few seconds, so you don't need to restart.
- Once you change something, a row of buttons asks what to do with it: **Save & Apply** saves and installs the layout, **Save as new** saves a copy under a new name, **Save only** saves without installing, and **Discard** drops the changes. The built-in layouts can't be changed, so saving one asks for a name for your copy. Picking another layout with unsaved changes asks first: **Save only** or **Discard**.
- **Manage Layouts…** lists every layout. Click one to open it in the editor. You can also install it, rename it, delete it (the bin, which always asks first; **Undo** brings it back) and drag your own into the order you want (or press Alt+↑ / Alt+↓ on one); the Layout menu follows that order. **Create new…** makes a layout from what's on screen. In the Mac app, **Manage Layouts…** is a menu: pick a layout to open it, and the items below act on the one you're editing. Deleting a layout from the editor doesn't remove it from the game folder: if it's installed, the game keeps using it until you install another, and the editor adds it back to your layouts the next time it opens.
- **Export…** shows the layout's settings file for you to copy or save with **Export as .txt**. Without the apps or the zip's scripts, you put that file in the game folder yourself.
- Undo and redo cover up to 50 steps: ⌘Z and ⇧⌘Z on a Mac, Ctrl+Z and Ctrl+Y on Windows, or the arrows next to **Manage Layouts…**.
- The four movement keys sit around a small stick in the **Movement** row. Its **Advanced…** has the speed-up, stopping and turn times, and the **Advanced…** under **Directional dodge** has the dodge's drag distance and stick time. **Tap vs hold** for the menu keys is at the end of **Menus**.
- In **Mod keys**, the options for when the cursor comes back sit under the cursor key: moving the mouse while you stand still, where it reappears, and keeping the mouse inside the game window. The switches for the typing banner and the "wasdmod off" banner sit under **Typing mode** and **Mod on / off**. **On-screen key list** and **Block inactive keys** are under the keyboard map.
- The checklist of in-game keys to change folds away under its heading, and **Details** folds the yellow notice's text. Folded, the checklist's heading still shows how many changes are left.
- **Customization** holds shortcuts of your own for game keys the other lists don't cover, such as a screenshot key. Turn on **Use my own shortcuts**, click **+** under **Always send to the game** or **Other menu shortcuts**, name the shortcut and give it your key. If the game uses a different key for it, set that as its in-game key and the mod sends it when you press yours. While the switch is off, the mod doesn't send them, and saving the layout leaves them out.
- Menus send the game's own keyboard shortcut. The inventory can use the controller instead: tap for the full inventory, or hold for the mini inventory while you keep moving. **Allow controller input for menus** gives the other menus and Teleport to player the same choice.

### The game's own keyboard settings

The game uses its own keyboard settings in menus, with the cursor and while you aim the bow, and its on-screen button prompts show them too. If they differ from your layout, you're playing with two layouts. The yellow notice in the editor lists what to change in **Settings → Controls → Keyboard** (both columns), starting with the menu wheel on Tab. Leave the Controller tab at its defaults.

In the Mac app, in the Windows app and with the zip's scripts, the editor reads the game's keyboard settings itself (`AppData\Local\Dungeons2\Saved\SaveGames\EnhancedInputUserSettings.sav`; on Linux, that folder inside the game's Proton prefix), so the "in-game controls" are the keys the game really has. When you change a key in the game's menu, the "in-game controls" follow a second or so after the game saves it, even while you're playing, and the layout in the game is saved again so the mod still sends your keys to the right game keys. A line at the top of the editor says this. To write your layout's keys into the game instead, close the game and click **Change them for me**, in the checklist or in the yellow notice. The editor asks you first.

The first write keeps your old settings as `EnhancedInputUserSettings.sav.wasdmod-backup`, and the game uses the new ones from its next start. A few rows the game hasn't saved yet (jump, artifacts 2 and 3, the guidance trail, teleports, map, emotes, event log) still have to be set in the game's menu.

It works the other way too: **Use in-game controls instead** copies the keys from the game's keyboard settings into your layout. A game key that's one of your movement keys is left out, and the editor tells you to change it in the game. The game's own Menu Wheel key, for example, is S, which is "move down" with WASD.

## Languages

The apps, the key layout editor and the on-screen key list come in the game's 16 languages.

- The apps and the editor start in your system's language. To change it, use the 🌐 menu in the window's toolbar on a Mac, or at the top of the window on Windows and on the web page.
- The on-screen key list follows the language set in the game. To pick another one, set `Language=` under `[Options]` in your layout file (`en`, `de`, `es`, `fr`, `it`, `ja`, `ko`, `nl`, `pl`, `pt`, `ru`, `sv`, `tr`, `uk`, `zh-Hans`, `zh-Hant`, or `Auto`).

The translations were written with AI help, so some wording may sound off, and the names of the game's own settings may not match the game exactly. Corrections are very welcome. Each language is one file in [`lang/`](lang), with English on the left and the translation on the right: edit it and open a pull request, or open an issue with the fix.

## Troubleshooting

### Nothing changes in game

Restart the game after installing, then press **F9**. If no key list appears, the mod isn't loaded.

- Windows: check that `xinput1_4.dll` is next to `Dungeons-Win64-Shipping.exe` (or `Dungeons-WinGDK-Shipping.exe` for the Minecraft Launcher and Xbox app). Antivirus may have removed it (see below).
- Linux / Steam Deck: start `wasdmod-linux.sh` again and read the line under the editor's buttons. If it shows a launch option, paste it in Steam under the game's **Properties → Launch Options**.
- Mac: click **Reinstall Mod** at the top right of the window (it also sets the CrossOver options), then restart the game.

### Antivirus flags wasdmod

wasdmod reads the keyboard and presses controller buttons, and it isn't signed, so to antivirus software it looks like other input tools. Restore the file and allow it, or build it yourself from this repository.

### Holding Option doesn't show the cursor (Mac)

Click **Reinstall Mod** at the top right of the wasdmod window, which sets CrossOver to send Option as Alt, and restart the game afterwards.

### Keys do their old thing in menus

The game's keyboard settings don't match your layout. The yellow notice in the editor lists what to change. In the apps, close the game and click **Change them for me** in that notice to have them set for you. Any rows the apps can't set stay in the list.

### Stuck without a cursor, or stuck typing

Hold **left Alt** for the cursor, press **Esc** to leave typing mode, or press **backtick (`)** to turn wasdmod off.

### Record logs for a bug report

1. Open wasdmod and click **Record logs**. It's under **Troubleshooting** at the end of the window.
2. Play until the problem happens.
3. Come back and click **Stop and save logs**. A `wasdmod-logs-….txt` file appears on your Desktop.
4. [Open an issue](https://github.com/Wanzho/mcd2-wasd/issues/new), describe what happened and attach the file.

The file has the mod's log (every key and mouse button wasdmod handled while recording, and what it did with it), your key layout and your system versions. Nothing you type is recorded, and keys your layout doesn't use show only as "other key".

If you installed by hand, attach `wasdmod.log` from the game's `Dungeons/Binaries/Win64` folder. For the detailed log, put an empty file named `wasdmod-record.flag` there while you play.

## Disable or uninstall

In a running game, press **backtick (`)**. While wasdmod is off, a "wasdmod off" banner shows the key that turns it back on (its **×** closes it). Press it again and "wasdmod on" shows for a moment; restarting the game also turns wasdmod back on.

To start the game without wasdmod until you enable it again (your layouts are kept):

- Windows: run **wasdmod** and click **Disable** at the top right of the window (later **Enable**).
- Mac: open **wasdmod** and click **Disable** at the top right of the window (later **Enable**).
- The zip: start `wasdmod-linux.sh` or `wasdmod-windows.cmd` and click **Disable** (later **Enable**).
- By hand: rename `xinput1_4.dll` to `xinput1_4.dll.off` (and back).

To remove it for good:

- Windows: run **wasdmod** and click **Uninstall**.
- Mac: open **wasdmod** and click **Uninstall**.
- The zip: start `wasdmod-linux.sh` or `wasdmod-windows.cmd` and click **Uninstall**.
- By hand: delete `xinput1_4.dll`, `default.txt`, `author.txt` and any `wasdmod*.txt` from `Dungeons/Binaries/Win64`, and remove the launch option.

Steam's "Verify integrity of game files" doesn't remove wasdmod.

## Tested

Tested on an M5 Pro with CrossOver 26.3, with the game set up by MCD2 Crossover.

## Limitations

On Windows, the app, its one-click update, the key layout editor and the in-game overlays have been tested on Windows 11 in a virtual machine. A player has reported playing it in the Steam version of the game on Windows, installed from the zip. Feedback is welcome. The Minecraft Launcher / Xbox app version of the game hasn't been tried, and Linux / Steam Deck and online co-op are untested.

The game loads wasdmod as its controller driver; it doesn't edit the game's files or memory. It's still a third-party file inside the game folder, so use it at your own risk.

## For developers

### Advanced settings

`default.txt` explains every option. A few worth knowing:

- `MouseMoveSwitches=0`: moving the mouse never leaves controller mode; only the cursor key, the menu keys or backtick give you the mouse.
- `CursorMode=Toggle`: the cursor key shows the cursor on one press and hides it on the next.
- `BowAimsWithMouse=1`: holding the bow button switches to mouse mode, so the game aims the bow at the cursor.
- `BumpNudgePct`: after a small mouse bump the game shows keyboard prompts, and wasdmod flips it back with a tiny right-stick nudge. Lower it if you ever dodge by accident.
- Key names: the left and right Shift, Ctrl and Alt are separate keys, `LShift`, `RShift`, `LCtrl`, `RCtrl`, `LAlt` and `RAlt` (on a Mac, `LAlt` and `RAlt` are the Option keys). A plain `Shift`, `Ctrl` or `Alt` is the left key, so the right one stays free for something else. The editor saves the full names. `LCmd` and `RCmd` (`LWin` and `RWin` on a PC) work in a layout file too; a Command key you bind that way stops counting as the "no key registers" key.
- `DisabledKeys` (filled in by the editor): the game's own keys for actions you moved to other keys. They never reach the game, in any mode. Left click is never disabled.

wasdmod uses the newest of `default.txt`, `author.txt` and any `wasdmod*.txt` in the game folder. It checks every second, so a newer file takes over in a running game. The editor saves `default.txt` or `author.txt` for an unchanged built-in layout, otherwise `wasdmod-MMDDYY.txt`.

The Windows app also has a command line. `/setup` opens the setup window instead of the editor window. `/find`, `/status`, `/install`, `/uninstall`, `/default`, `/recommended`, `/own`, `/off` and `/on` work without a window. Each can be followed by the game's folder or executable. The zip's scripts take `status`, `install`, `uninstall`, `off`, `on` and `find` the same way, and `--no-open` runs only the editor's server and prints its address.

### How it works

The DLL stands in for XInput: it reports controller 0 built from the keyboard and mouse, and forwards real controllers to the system's XInput. A message hook on the game's window threads hides the mapped keys and mouse (including the raw-input copies the game also reads), so the game sees only the controller and doesn't flicker between keyboard and controller mode. Text boxes are detected through Text Services (IMM as a fallback) and get the keyboard back.

Tried and dropped: patching the game's `GetCursorPos` import (the game quits 20 to 30 s later), a moving overlay window (lag under CrossOver) and `ClipCursor` (confined the mouse to the wrong area).

### Build and test

`./build.sh` (Apple clang, the lld-link from a Rust toolchain, and Xcode's Swift for the Mac app) builds:

- `build/xinput1_4.dll` and `build/test_load.exe`;
- `Keybinder.html` (from `configurator.html`, with every language from `lang/` built in);
- the downloads in `dist/`, named with `VERSION` from `build.sh`:
  - `wasdmod-VERSION.exe`, the Windows app (`installer.c`);
  - `wasdmod-VERSION.dmg`, the Mac app (`mac/main.swift` over `mac/wasdmod.sh`), with its designed window from `mac/dmg/` made by `mac/make-dmg.sh`;
  - `wasdmod-VERSION.zip`, for Linux, Steam Deck and Windows without the app: the mod's files with two plain scripts, `manual/wasdmod.py` (started by `wasdmod-linux.sh`) and `manual/wasdmod.ps1` (started by `wasdmod-windows.cmd`). They serve the editor on 127.0.0.1 the way the Windows app does and answer the same requests, which are listed in the first script of `configurator.html`.

The first build downloads Microsoft's WebView2 SDK from nuget.org (checked against its SHA-256) for the loader of the Windows editor window. It also downloads [dmgbuild](https://github.com/dmgbuild/dmgbuild) from PyPI (pinned with hashes in `mac/dmg/requirements.txt`, into a virtualenv in `~/Library/Caches/wasdmod-build`; needs Python 3.10 or newer), which lays out the disk image's window without opening Finder. The window's picture is `mac/dmg/background.html`; `sh mac/dmg/render.sh` renders it again with headless Chrome.

GitHub Actions runs the same build on every push (`.github/workflows/build.yml`) and attaches the files to each published release. Code signing of the Windows app through SignPath is prepared in the workflow but not active yet; see [`.signpath/README.md`](.signpath/README.md).

`FLAVOR=nexus sh build.sh` builds the Nexus Mods files into `dist/nexus/`. They are the same apps with the update code left out when compiling (`-DNEXUS`, `-D NEXUS`; WinHTTP isn't linked, and the editor doesn't load web fonts) and the zip's two scripts without the lines between `#if !NEXUS` and `#endif`, with the Mac disk image zipped as `wasdmod-VERSION-mac.zip` and all three files in `wasdmod-VERSION-nexus.zip`. GitHub Actions builds both flavors. For a release (tagged `vVERSION`) it also uploads `SHA256SUMS` (made after signing) and `wasdmod-VERSION-nexus.zip`. The updater's parts are `mac/updater.swift` and, for Windows, `updater.inc` (WinHTTP, replacing the exe) and `release.inc` (the release's JSON, versions, SHA-256, allowed addresses; `release_test.c` tests it on the host).

`lang.py` builds the translations into each part; `python3 lang.py --check` lists text that a language doesn't translate yet. In the code, English text is the key: `t("…")` in the editor, `L("…")` in the Mac app, `T("…")` in the DLL, the Windows app and `wasdmod.py`, and `T '…'` in `wasdmod.ps1`. The two scripts read their text from `lang.json` beside them; what their commands print stays English.

No Windows SDK or C runtime is needed. `install.command` / `uninstall.command` install from the source folder.

`build/test_load.exe` runs inside a Wine bottle and checks the exports and the input filtering (gameplay, typing, menus, remaps, bow, text boxes). `smooth_test.c` and `dodge_test.c` test the movement smoothing and the drag dodge on the host.

## License

[MIT](LICENSE).

The "wasdmod" wordmark in the Windows app's setup window and the headings in the key layout editor use [Mojang by b.tenthousand](https://fontstruct.com/fontstructions/show/836974) (CC0 public domain), with some glyphs and the spacing adjusted to match Minecraft's lettering, and Cyrillic, Polish, Turkish and Portuguese letters added on the same pixel grid.

The Windows app includes Microsoft's `WebView2Loader.dll` from the [WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2), which starts its window. Its license:

> Copyright (C) Microsoft Corporation. All rights reserved.
>
> Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
>
> * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
> * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
> * The name of Microsoft Corporation, or the names of its contributors may not be used to endorse or promote products derived from this software without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Unofficial project. NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT. Not affiliated with CodeWeavers or Valve.

## References

Coded with Claude Opus 5.5.
