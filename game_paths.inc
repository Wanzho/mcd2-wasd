// Game-folder resolution, shared by the picker, automatic search and command line.
// gamePathKind returns 0 for missing, 1 for a file, 2 for a directory.
// Keep the actual runtime names explicit: launchers and unrelated .exe files are
// only a starting point for the search, never the destination for the mod.
static const char *gameExecutables[] = {
    "Dungeons-Win64-Shipping.exe", "Dungeons-WinGDK-Shipping.exe"
};
static int gameExecutableName(const char *name) {
    for (unsigned i = 0; i < sizeof(gameExecutables) / sizeof(*gameExecutables); i++)
        if (sameText(name, gameExecutables[i])) return 1;
    return 0;
}
static int gameJoin(char *out, const char *dir, const char *name) {
    if (slen(dir) + slen(name) + 2 > PATHLEN) return 0;
    scpy(out, PATHLEN, dir);
    unsigned n = slen(out);
    if (n && out[n - 1] != '\\' && out[n - 1] != '/') cat(out, PATHLEN, "\\");
    cat(out, PATHLEN, name);
    return 1;
}
static int isGameDir(const char *dir) {
    if (gamePathKind(dir) != 2) return 0;
    for (unsigned i = 0; i < sizeof(gameExecutables) / sizeof(*gameExecutables); i++) {
        char exe[PATHLEN];
        if (gameJoin(exe, dir, gameExecutables[i]) && gamePathKind(exe) == 1) return 1;
    }
    return 0;
}
static int gameAt(const char *root, char *out) {
    static const char *subs[] = {
        "", "Dungeons\\Binaries\\Win64", "Dungeons\\Binaries\\WinGDK",
        "Content\\Dungeons\\Binaries\\Win64", "Content\\Dungeons\\Binaries\\WinGDK",
        "Binaries\\Win64", "Binaries\\WinGDK", "Win64", "WinGDK"
    };
    for (unsigned i = 0; i < sizeof(subs) / sizeof(*subs); i++) {
        char dir[PATHLEN];
        if (!subs[i][0]) scpy(dir, PATHLEN, root);
        else if (!gameJoin(dir, root, subs[i])) continue;
        if (isGameDir(dir)) { scpy(out, PATHLEN, dir); return 1; }
    }
    return 0;
}
static int gameParent(char *path) {
    unsigned n = slen(path);
    while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
    if (!n || (n == 3 && path[1] == ':') || n == 1) return 0;
    path[n - 1] = 0;
    return 1;
}
static int dirFromPick(const char *pick, char *out) {
    char root[PATHLEN];
    if (!pick[0] || slen(pick) >= PATHLEN) return 0;
    scpy(root, PATHLEN, pick);
    unsigned n = slen(root);
    while (n > 3 && (root[n - 1] == '/' || root[n - 1] == '\\')) root[--n] = 0;
    int kind = gamePathKind(root);
    if (kind == 1) {
        if (n < 4 || !sameText(root + n - 4, ".exe") || !gameParent(root)) return 0;
    } else if (kind != 2) return 0;
    // Check the selected folder, then nearby parents for launcher/helper selections.
    // No recursive drive scan, no acceptance of an arbitrary .exe on its own.
    for (int up = 0; up <= 4; up++) {
        if (gameAt(root, out)) return 1;
        if (!gameParent(root)) break;
    }
    return 0;
}
