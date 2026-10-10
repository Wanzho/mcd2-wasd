# Contributing to wasdmod

Thanks for wanting to help. Bug reports, translation fixes and code changes are all welcome. This page says how to send each one so it can be used.

By taking part you agree to the [code of conduct](CODE_OF_CONDUCT.md). Anything you contribute is released under the project's [MIT license](LICENSE).

## Report a bug

1. Check [Troubleshooting](README.md#troubleshooting) in the README first. Press F9 in the game: if no key list appears, the mod isn't loaded, and the README says what to try.
2. In the wasdmod app, click **Record logs**, play until the problem happens, then come back and stop the recording. A `wasdmod-logs-….txt` file appears on your Desktop. With a manual install, use `wasdmod.log` from the game's `Dungeons/Binaries/Win64` folder.
3. [Open a bug report](https://github.com/Wanzho/mcd2-wasd/issues/new/choose) and attach the file.

The log lists the keys and mouse buttons wasdmod handled and what it did with them, your key layout and your system versions. Nothing you type is recorded.

A good report says what you pressed, what you expected and what happened instead. It also says which copy of the game you have (Steam, Minecraft Launcher or Xbox app) and whether you play on Windows, a Mac with CrossOver, or Linux with Proton.

## Suggest a change

Open a [feature request](https://github.com/Wanzho/mcd2-wasd/issues/new/choose) and describe the problem you ran into while playing. A request that starts from a real situation is easier to judge than a list of options to add.

wasdmod plays the game as a controller. Requests that need the game's own files or memory changed are out of scope.

## Fix a translation

The translations were written with AI help, so corrections from native speakers are very welcome.

Each language is one file in [`lang/`](lang), with the English text on the left and the translation on the right. Edit the file on GitHub and open a pull request, or open a translation issue with the corrected lines if you'd rather not edit files. Keep the names of keys and of the game's own settings the way the game shows them in your language.

## Change the code

[For developers](README.md#for-developers) in the README explains how the mod works and how to build it. In short:

- `./build.sh` builds everything on a Mac (Apple clang, the `lld-link` from a Rust toolchain, Xcode's Swift). GitHub Actions runs the same build on every push, so a pull request gets built even if you can't build locally.
- The DLL and the Windows app are written without a C runtime or the Windows SDK. Follow the code around your change: the same helpers, the same comment style, no new dependencies.
- English text is the key for translations: `t("…")` in the editor, `L("…")` in the Mac app, `T("…")` in the DLL and the Windows app. If you add or change text, add it to every file in `lang/` and run `python3 lang.py --check`. A machine translation is fine as a start if you say so in the pull request.
- `configurator.html` is the editor for the Mac app, the Windows app and the plain web page. A change there has to keep all three working.
- Test what you changed and say how in the pull request. `build/test_load.exe` checks the input handling inside a Wine bottle. `smooth_test.c` and `dodge_test.c` run on the host.

Keep a pull request to one thing. Small ones get reviewed sooner. For anything large, open an issue first so we can agree on the approach before you write it.

AI-assisted contributions are fine. wasdmod itself was coded with AI help. Say so in the pull request, and only send code you have read and tried.

## Security problems

Please don't report a security problem in a public issue. See the [security policy](SECURITY.md).
