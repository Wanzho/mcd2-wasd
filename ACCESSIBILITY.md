# Accessibility

wasdmod changes how you control Minecraft Dungeons II, so parts of it may help some players and get in the way of others. This page says what has been done, what is known not to work, and how to report a barrier.

## What has been done

- Every action can be put on any key or mouse button, so you can build a layout around the keys you can reach. The editor's keyboard map shows what each key does.
- Holding a key isn't required for the cursor: `CursorMode=Toggle` shows it on one press and hides it on the next.
- How quickly movement speeds up, stops and turns, and how far you drag for a directional dodge, can be tuned in the editor.
- The editor's buttons and key slots have text labels for screen readers.
- The editor and the website turn their animations off when your system asks for reduced motion.
- The apps, the editor and the on-screen key list come in the game's 16 languages.

## Known limitations

- Nothing here has been tested with a screen reader or other assistive technology. The labels are there, but nobody has checked how well they read.
- The on-screen key list and the banners in the game are drawn as pictures. A screen reader can't read them.
- The editor marks key categories with colour. Each key also has its action in a tooltip and in the list, but the keyboard map itself leans on colour.
- wasdmod needs a keyboard and a mouse. It doesn't replace the game's own accessibility settings and can't change what the game itself shows or says.

## Report a barrier

If something in wasdmod is hard or impossible for you to use, [open an issue](https://github.com/Wanzho/mcd2-wasd/issues/new/choose) and say what you were trying to do, what got in the way and what you use (system, screen reader or other tools, if any). These reports are treated as bugs.
