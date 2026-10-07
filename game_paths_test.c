// Host regression tests for the installer's production resolver.
// clang -Wall -Wextra game_paths_test.c -o build/game_paths_test && build/game_paths_test
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define PATHLEN 600
static unsigned slen(const char *s) { return (unsigned)strlen(s); }
static void scpy(char *d, unsigned cap, const char *s) { snprintf(d, cap, "%s", s); }
static void cat(char *d, unsigned cap, const char *s) { strncat(d, s, cap - strlen(d) - 1); }
static int sameText(const char *a, const char *b) { return !strcasecmp(a, b); }
static const char *files[12], *dirs[12];
static int pathEqual(const char *a, const char *b) {
    char x[PATHLEN], y[PATHLEN]; scpy(x, sizeof(x), a); scpy(y, sizeof(y), b);
    for (char *p = x; *p; p++) if (*p == '/') *p = '\\';
    for (char *p = y; *p; p++) if (*p == '/') *p = '\\';
    return sameText(x, y);
}
static int gamePathKind(const char *p) {
    for (int i = 0; dirs[i]; i++) if (pathEqual(dirs[i], p)) return 2;
    for (int i = 0; files[i]; i++) if (pathEqual(files[i], p)) return 1;
    return 0;
}
#include "game_paths.inc"
static int checks;
static void expect(const char *pick, const char *want) {
    char result[PATHLEN] = "unchanged";
    int found = dirFromPick(pick, result);
    if (found != (want != NULL) || (found && !pathEqual(result, want))) {
        fprintf(stderr, "FAIL %s: %d %s (wanted %s)\n", pick, found, result, want ? want : "rejection");
        assert(0);
    }
    checks++;
}
int main(void) {
    for (int platform = 0; platform < 2; platform++) for (int content = 0; content < 2; content++) {
        char root[PATHLEN] = "D:\\Custom Games\\Minecraft Dungeons II", bin[PATHLEN], exe[PATHLEN], launcher[PATHLEN];
        char binaries[PATHLEN], engine[PATHLEN], helper[PATHLEN], folderFake[PATHLEN];
        snprintf(bin, sizeof(bin), "%s\\%sDungeons\\Binaries\\%s", root, content ? "Content\\" : "", platform ? "WinGDK" : "Win64");
        snprintf(exe, sizeof(exe), "%s\\%s", bin, gameExecutables[platform]);
        snprintf(launcher, sizeof(launcher), "%s\\%sDungeons.exe", root, content ? "Content\\" : "");
        snprintf(binaries, sizeof(binaries), "%s\\%sDungeons\\Binaries", root, content ? "Content\\" : "");
        snprintf(engine, sizeof(engine), "%s\\Engine\\Binaries\\Win64", root);
        snprintf(helper, sizeof(helper), "%s\\gamelaunchhelper.exe", engine);
        snprintf(folderFake, sizeof(folderFake), "C:\\Not a game\\%s", gameExecutables[platform]);
        files[0]=exe; files[1]=launcher; files[2]=helper; files[3]="C:\\Other\\Dungeons.exe"; files[4]="C:\\Other\\Notes.txt"; files[5]=NULL;
        dirs[0]=root; dirs[1]=bin; dirs[2]=binaries; dirs[3]=engine; dirs[4]="C:\\Not a game"; dirs[5]=folderFake; dirs[6]=NULL;
        expect(root, bin); expect(bin, bin); expect(exe, bin); expect(launcher, bin); expect(binaries, bin); expect(helper, bin);
        expect("C:\\Not a game", NULL); expect("C:\\Other\\Dungeons.exe", NULL); expect("C:\\Other\\Notes.txt", NULL);
        expect("C:\\Missing\\Dungeons-WinGDK-Shipping.exe", NULL); expect("", NULL);
        char lower[PATHLEN]; scpy(lower, sizeof(lower), exe);
        for (char *p=lower; *p; p++) { if (*p=='\\') *p='/'; else if (*p>='A' && *p<='Z') *p += 'a'-'A'; }
        expect(lower, bin);
        char trailing[PATHLEN]; snprintf(trailing, sizeof(trailing), "%s\\", bin); expect(trailing, bin);
    }
    // Choosing one runtime explicitly must not select a sibling platform.
    files[0]="C:\\Both\\Dungeons\\Binaries\\Win64\\Dungeons-Win64-Shipping.exe";
    files[1]="C:\\Both\\Dungeons\\Binaries\\WinGDK\\Dungeons-WinGDK-Shipping.exe"; files[2]=NULL;
    dirs[0]="C:\\Both"; dirs[1]="C:\\Both\\Dungeons\\Binaries\\Win64"; dirs[2]="C:\\Both\\Dungeons\\Binaries\\WinGDK"; dirs[3]=NULL;
    expect(files[1], dirs[2]); expect(files[0], dirs[1]); expect(dirs[0], dirs[1]);
    char tooLong[PATHLEN+100]; memset(tooLong, 'a', sizeof(tooLong)-1); tooLong[sizeof(tooLong)-1]=0; expect(tooLong, NULL);
    assert(gameExecutableName("DUNGEONS-WINGDK-SHIPPING.EXE"));
    assert(gameExecutableName("Dungeons-Win64-Shipping.exe"));
    assert(!gameExecutableName("gamelaunchhelper.exe"));
    assert(!gameExecutableName("Other-Win64-Shipping.exe"));
    printf("PASS %d folder-resolution cases and process-name checks\n", checks);
}
