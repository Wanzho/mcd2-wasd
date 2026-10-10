# Security policy

## Supported versions

Only the newest release gets fixes. If you're on an older version, update first and check whether the problem is still there.

## Report a vulnerability

Please don't open a public issue for a security problem.

Report it privately on GitHub: open the repository's [Security tab](https://github.com/Wanzho/mcd2-wasd/security) and click **Report a vulnerability**. Say what the problem is, how to reproduce it, and which version and system you used.

This is a one-person hobby project, so there is no guaranteed response time. You'll get an answer as soon as the report has been read, and credit in the release notes if you want it.

## What counts

wasdmod is a file the game loads, an app that installs it, and a key layout editor. Reports about any of these are in scope:

- the mod doing something with your keys or mouse outside the game;
- the apps' local editor page being reachable or controllable by another program or website;
- the update check or one-click update accepting a file it shouldn't;
- the apps writing or deleting files outside the game folder, their own settings and the files you pick;
- anything that lets a layout file or a translation file run code.

## What doesn't

- Antivirus warnings about the unsigned Windows app or about `xinput1_4.dll`. The README's [Troubleshooting](README.md#troubleshooting) section explains them. If you think a specific detection is right, that is worth a report.
- Problems in the game itself, in CrossOver, Proton or Steam.
- Cheating in online play. wasdmod only acts as a controller, and requests to make it do more are declined.
