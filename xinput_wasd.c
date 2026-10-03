// Keyboard & mouse as a controller for Minecraft Dungeons II.
//
// Drop-in xinput1_4.dll. Keys and mouse buttons from default.txt are reported
// as controller 0 (sticks, buttons, triggers), and hidden from the game, so the
// game only ever sees a controller and never flips between keyboard and
// controller mode. A small on-screen legend shows which key does what. Text
// boxes are detected and get the keyboard back while focused. Real controllers
// are forwarded to Wine's (or Windows') own XInput. The game is not modified.
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef short SHORT;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned int UINT;
typedef int BOOL;
typedef void *HANDLE;
typedef unsigned short WCHAR;
typedef long long LRESULT;
typedef unsigned long long WPARAM;
typedef long long LPARAM;
typedef unsigned long long U64;
#define IMP __declspec(dllimport)
#define EXP __declspec(dllexport)

// kernel32
IMP HANDLE LoadLibraryA(const char *);
IMP BOOL FreeLibrary(HANDLE);
IMP UINT GetSystemDirectoryA(char *, UINT);
IMP void *GetProcAddress(HANDLE, const char *);
IMP DWORD GetModuleFileNameA(HANDLE, char *, DWORD);
IMP UINT GetPrivateProfileIntA(const char *, const char *, int, const char *);
IMP DWORD GetPrivateProfileStringA(const char *, const char *, const char *, char *, DWORD, const char *);
IMP DWORD GetPrivateProfileSectionA(const char *, char *, DWORD, const char *);
typedef struct { DWORD attributes; DWORD created[2], accessed[2], written[2]; DWORD sizeHigh, sizeLow; } FILEINFO;
IMP BOOL GetFileAttributesExA(const char *, int, FILEINFO *);
typedef struct { DWORD attributes; DWORD created[2], accessed[2], written[2]; DWORD sizeHigh, sizeLow, reserved0, reserved1; char name[260]; char shortName[14]; } FINDDATA;
IMP HANDLE FindFirstFileA(const char *, FINDDATA *);
IMP BOOL FindNextFileA(HANDLE, FINDDATA *);
IMP BOOL FindClose(HANDLE);
IMP DWORD GetCurrentProcessId(void);
IMP U64 GetTickCount64(void);
IMP HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
IMP BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
IMP BOOL CloseHandle(HANDLE);
IMP HANDLE CreateThread(void *, U64, DWORD (*)(void *), void *, DWORD, DWORD *);
IMP BOOL QueryPerformanceCounter(long long *);
IMP BOOL QueryPerformanceFrequency(long long *);
IMP HANDLE GetModuleHandleA(const char *);
IMP DWORD GetFileAttributesA(const char *);
IMP BOOL MoveFileExA(const char *, const char *, DWORD);
typedef struct { WORD year, month, weekday, day, hour, minute, second, ms; } SYSTEMTIME;
IMP void GetLocalTime(SYSTEMTIME *);
// user32
IMP SHORT GetAsyncKeyState(int);
IMP HANDLE GetForegroundWindow(void);
IMP DWORD GetWindowThreadProcessId(HANDLE, DWORD *);
IMP HANDLE SetWindowsHookExW(int, LRESULT (*)(int, WPARAM, LPARAM), HANDLE, DWORD);
IMP BOOL EnumWindows(BOOL (*)(HANDLE, LPARAM), LPARAM);
IMP BOOL TranslateMessage(const void *);
IMP HANDLE GetFocus(void);
IMP void keybd_event(BYTE, BYTE, DWORD, U64);
IMP void mouse_event(DWORD, DWORD, DWORD, DWORD, U64);
IMP UINT MapVirtualKeyA(UINT, UINT);

#define ERROR_SUCCESS 0
#define ERROR_DEVICE_NOT_CONNECTED 1167
#define ERROR_EMPTY 4306

typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD;
typedef struct { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; } XINPUT_STATE;
typedef struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } XINPUT_VIBRATION;
typedef struct { BYTE Type, SubType; WORD Flags; XINPUT_GAMEPAD Gamepad; XINPUT_VIBRATION Vibration; } XINPUT_CAPABILITIES;
typedef struct { BYTE BatteryType, BatteryLevel; } XINPUT_BATTERY_INFORMATION;

void *memset(void *p, int v, U64 n) { volatile BYTE *c = p; while (n--) *c++ = (BYTE)v; return p; }
void *memcpy(void *d, const void *s, U64 n) { volatile BYTE *o = d; const BYTE *i = s; while (n--) *o++ = *i++; return d; }
int _fltused = 0; // floating point without the C runtime
#include "smooth.inc"
#include "dodge.inc"

static unsigned len(const char *s) { unsigned n = 0; if (s) while (s[n]) n++; return n; }
static void append(char *b, unsigned cap, const char *s) { unsigned n = len(b); if (s) while (*s && n + 1 < cap) b[n++] = *s++; b[n] = 0; }
static void appendNum(char *b, unsigned cap, U64 v) { char t[24]; int k = 0; do t[k++] = '0' + v % 10; while ((v /= 10) && k < 22); char r[24]; int j = 0; while (k) r[j++] = t[--k]; r[j] = 0; append(b, cap, r); }
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int named(const char *s, const char *name) { while (*name) if (lower(*s++) != *name++) return 0; return !*s; }

static HANDLE self, real;
static char dir[1024], ini[1100];
static long long perfFreq; // QueryPerformanceCounter ticks per second

// ---------------------------------------------------------------- settings

// Controller inputs the keyboard can drive. Order matters for the legend.
enum { B_A, B_B, B_X, B_Y, B_LB, B_RB, B_LT, B_RT, B_BACK, B_START, B_LS, B_RS, B_DUP, B_DDOWN, B_DLEFT, B_DRIGHT, NBTN };
static const char *btnName[NBTN] = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Back", "Start", "LS", "RS", "DUp", "DDown", "DLeft", "DRight"};
static const WORD btnBit[NBTN] = {0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0, 0, 0x20, 0x10, 0x40, 0x80, 1, 2, 4, 8};
// For a setting a layout leaves out: default.txt's value (the game's own keyboard
// keys played as a controller). Directional dodge (right stick) is under [Move].
static const char *btnDefaultKeys[NBTN] = {"Space", "2", "Mouse1", "1", "Mouse5", "3", "E", "Mouse2", "None", "None", "Mouse3", "None", "None", "None", "None", "None"};
static const char *btnDefaultLabel[NBTN] = {"Jump / interact", "Artifact 2", "Melee (in the air: heavy jump attack)", "Artifact 1", "Forward dodge", "Artifact 3", "Health potion", "Ranged (bow)", "World map", "Menu wheel / event log", "Guidance trail", "Emotes", "Inventory (tap: full, hold: mini)", "Social menu", "Teleport to player", "Track quest / quest log"};

#define MAXKEYS 3
typedef struct { BYTE vk[MAXKEYS]; char text[48]; } Binding;
static Binding btn[NBTN], moveUp, moveDown, moveLeft, moveRight, dodgeMouse, toggleKey, legendKey, cursorKey;
// The game's own menu shortcuts: passed to the game untouched, and they switch the
// mod to mouse mode (so the menu opens with a cursor). Any number of keys.
static BYTE menuVk[256]; static char menuText[128];
// Keys that always reach the game unchanged (no mode switch). Every other key the
// mod doesn't use is blocked in controller mode (BlockOtherKeys), so the game
// never sees keyboard input there.
static BYTE passVk[256];
static BYTE disabledVk[256]; // the game's own key for an action you moved to another key: never reaches the game
static BYTE instantVk[256];  // game keys of instant actions (teleports): a key remapped onto one doesn't open mouse mode
// Back keys (Esc): in mouse mode they close the menu (the game gets them) and
// switch straight back to controller mode.
static BYTE backVk[256];
static BYTE bowVk[256]; // mouse buttons that aim the bow with the mouse (keyboard mode)
// [Remap]: pressing the key sends the game a different key instead (B=U: B
// opens collectibles). The original key is hidden; the sent key is a real key
// press, so menu keys among them still switch to mouse mode.
static BYTE remapTo[256]; static char remapText[200];
// The same remaps for the key list: "Tab" sends "S".
#define MAXREMAP 12
static struct { char from[24], to[24]; BYTE toVk; } remapList[MAXREMAP]; static int remapCount;
// Key presses the mod itself sent (remaps) are let through for a moment, even
// if that key is otherwise blocked.
static U64 sentUntil[256];
// Typing mode key(s): everything reaches the game until pressed again (or Esc).
static BYTE typeVk[256]; static char typeText[32];
static char btnLabel[NBTN][40];
static BYTE mouseAfter[NBTN]; // buttons that open a menu: mouse mode once released
static volatile int suppressMouseAfter; // this press closes the menu instead

static struct {
    int requireFocus, alwaysConnected, log, detectText, hideMouse, legendSeconds, legend, typedChars, blockOtherKeys;
    int accelMs, decelMs, turnMs;
    int dodgeDragPx, dodgeFlickMs, menuTapMs, mouseWakePx, mouseMoveSwitches, bumpNudgePct;
    int bowMouseAim, cursorToggle;
    int clipCursor, cursorFromMiddle;
} cfg;

#ifndef VERSION
#define VERSION "dev"
#endif
static volatile unsigned logBytes; // written this session (Record Logs stops at a limit)
static void two(char *o, unsigned v) { o[0] = (char)('0' + v / 10 % 10); o[1] = (char)('0' + v % 10); }
// Each line starts with the time: "12:34:56.789 Mouse mode (menu key)".
static void logline(const char *s) {
    if (!cfg.log) return;
    char path[1100] = {0}; append(path, sizeof(path), dir); append(path, sizeof(path), "wasdmod.log");
    HANDLE h = CreateFileA(path, 4 /*FILE_APPEND_DATA*/, 3, 0, 4 /*OPEN_ALWAYS*/, 128, 0);
    if (h == (HANDLE)-1) return;
    SYSTEMTIME t; GetLocalTime(&t);
    char stamp[16] = "00:00:00.000 ";
    two(stamp, t.hour); two(stamp + 3, t.minute); two(stamp + 6, t.second);
    stamp[9] = (char)('0' + t.ms / 100 % 10); two(stamp + 10, t.ms % 100);
    DWORD w; WriteFile(h, stamp, 13, &w, 0); WriteFile(h, s, len(s), &w, 0); WriteFile(h, "\r\n", 2, &w, 0); CloseHandle(h);
    logBytes += 15 + len(s);
}

// A letter/digit ("W"), a key name ("Space", "Mouse1", "F5"), "None", or a
// virtual-key code ("0x20").
static int keyCode(const char *v) {
    if (!v[0] || named(v, "none")) return 0;
    if (!v[1]) {
        int c = v[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
        if (c == '`') return 0xC0;
        if (c == '-') return 0xBD; if (c == '=') return 0xBB; if (c == '[') return 0xDB; if (c == ']') return 0xDD;
        if (c == ';') return 0xBA; if (c == '\'') return 0xDE; if (c == ',') return 0xBC; if (c == '.') return 0xBE; if (c == '/') return 0xBF;
    }
    static const struct { const char *name; BYTE vk; } names[] = {
        {"space", 0x20}, {"enter", 0x0D}, {"return", 0x0D}, {"escape", 0x1B}, {"esc", 0x1B}, {"tab", 0x09},
        {"shift", 0x10}, {"lshift", 0xA0}, {"rshift", 0xA1}, {"ctrl", 0x11}, {"control", 0x11}, {"alt", 0x12},
        {"backspace", 0x08}, {"capslock", 0x14}, {"up", 0x26}, {"down", 0x28}, {"left", 0x25}, {"right", 0x27},
        {"mouse1", 0x01}, {"mouse2", 0x02}, {"mouse3", 0x04}, {"mouse4", 0x05}, {"mouse5", 0x06},
        {"backtick", 0xC0}, {"grave", 0xC0}, {"tilde", 0xC0},
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) if (named(v, names[i].name)) return names[i].vk;
    if (lower(v[0]) == 'f' && v[1] >= '1' && v[1] <= '9') {
        int f = v[1] - '0';
        if (v[2] >= '0' && v[2] <= '2' && !v[3]) f = f * 10 + v[2] - '0'; else if (v[2]) f = 0;
        if (f >= 1 && f <= 12) return 0x6F + f;
    }
    int base = 10, n = 0; const char *p = v;
    if (p[0] == '0' && lower(p[1]) == 'x') { base = 16; p += 2; }
    for (; *p; p++) {
        int c = lower(*p), d = c >= '0' && c <= '9' ? c - '0' : base == 16 && c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return -1;
        n = n * base + d;
    }
    return n > 0 && n < 256 ? n : -1;
}

// "Space, Enter" -> up to two keys; the text is kept for the legend.
static void readBinding(const char *section, const char *name, const char *fallback, Binding *b) {
    char v[64];
    GetPrivateProfileStringA(section, name, fallback, v, sizeof(v), ini);
    memset(b, 0, sizeof(*b));
    int count = 0; char *p = v;
    while (*p && count < MAXKEYS) {
        while (*p == ' ' || *p == ',') p++;
        char *start = p; while (*p && *p != ',') p++;
        char *end = p; while (end > start && end[-1] == ' ') end--;
        char save = *end; *end = 0;
        int vk = keyCode(start);
        if (vk > 0) {
            b->vk[count++] = (BYTE)vk;
            if (b->text[0]) append(b->text, sizeof(b->text), " / ");
            append(b->text, sizeof(b->text), start);
        } else if (vk < 0) {
            char msg[128] = "Unknown key name in the settings: "; append(msg, sizeof(msg), start); logline(msg);
        }
        *end = save;
    }
}

static void readButtonList(const char *section, const char *name, const char *fallback, BYTE *flags);

// "Tab, I, M" under [Keys] -> flags per virtual-key code (any number of keys).
static void readKeyList(const char *name, const char *fallback, BYTE *flags, char *text, unsigned cap) {
    char v[160], *p = v;
    GetPrivateProfileStringA("Keys", name, fallback, v, sizeof(v), ini);
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        char *start = p; while (*p && *p != ',') p++;
        char *end = p; while (end > start && end[-1] == ' ') end--;
        char save = *end; *end = 0;
        int vk = keyCode(start);
        if (vk > 0) { flags[vk] = 1; if (text) { if (text[0]) append(text, cap, " "); append(text, cap, start); } }
        *end = save;
    }
}

static void loadSettings(void) {
    DWORD n = GetModuleFileNameA(self, dir, sizeof(dir));
    while (n && dir[n - 1] != '\\' && dir[n - 1] != '/') n--;
    dir[n] = 0;
    // Settings: default.txt (the installed defaults), author.txt, or a layout the
    // key editor saved (wasdmod-093026.txt...) -- whichever was changed most
    // recently. Files from older versions (wasdmod.ini, wasd-mod...) still count.
    {
        const char *names[] = {"default.txt", "author.txt", "wasdmod*.txt", "wasdmod.ini", "wasd-mod.ini", "wasd-mod*.txt"};
        DWORD best[2] = {0, 0}; int found = 0;
        ini[0] = 0; append(ini, sizeof(ini), dir); append(ini, sizeof(ini), "default.txt");
        for (int i = 0; i < 6; i++) {
            char pattern[1100] = {0}; append(pattern, sizeof(pattern), dir); append(pattern, sizeof(pattern), names[i]);
            FINDDATA f; HANDLE h = FindFirstFileA(pattern, &f);
            if (h == (HANDLE)-1) continue;
            do {
                if (f.attributes & 0x10 /*directory*/) continue;
                if (!found || f.written[1] > best[1] || (f.written[1] == best[1] && f.written[0] > best[0])) {
                    best[0] = f.written[0]; best[1] = f.written[1]; found = 1;
                    ini[0] = 0; append(ini, sizeof(ini), dir); append(ini, sizeof(ini), f.name);
                }
            } while (FindNextFileA(h, &f));
            FindClose(h);
        }
    }
    cfg.log = GetPrivateProfileIntA("Options", "Log", 1, ini);
    cfg.requireFocus = GetPrivateProfileIntA("Options", "RequireFocus", 1, ini);
    cfg.alwaysConnected = GetPrivateProfileIntA("Options", "AlwaysConnected", 1, ini);
    cfg.detectText = GetPrivateProfileIntA("Options", "DetectTextBoxes", 1, ini);
    cfg.hideMouse = GetPrivateProfileIntA("Options", "HideMouse", 1, ini);
    cfg.legend = GetPrivateProfileIntA("Options", "Legend", 1, ini);
    cfg.legendSeconds = GetPrivateProfileIntA("Options", "LegendSeconds", 15, ini);
    readBinding("Move", "Up", "W", &moveUp);
    readBinding("Move", "Down", "S", &moveDown);
    readBinding("Move", "Left", "A", &moveLeft);
    readBinding("Move", "Right", "D", &moveRight);
    // Click = dodge toward travel; press, drag, release = roll in the drag direction.
    readBinding("Move", "DodgeMouse", "R, Mouse4", &dodgeMouse);
    cfg.dodgeDragPx = GetPrivateProfileIntA("Move", "DodgeDragPx", 40, ini);
    cfg.dodgeFlickMs = GetPrivateProfileIntA("Move", "DodgeFlickMs", 80, ini);
    readBinding("Keys", "Toggle", "Backtick", &toggleKey);
    readBinding("Keys", "Legend", "F9", &legendKey);
    readBinding("Keys", "Cursor", "Alt", &cursorKey);
    {   // Hold: the cursor shows while the key is held. Toggle: one press shows it,
        // the next press locks it again.
        char v[16]; GetPrivateProfileStringA("Keys", "CursorMode", "Hold", v, sizeof(v), ini);
        cfg.cursorToggle = (v[0] | 32) == 't';
    }
    readKeyList("MenuKeys", "I, M, J, F, G, U, K, Escape", menuVk, menuText, sizeof(menuText));
    readKeyList("PassKeys", "Z, F1, F2, F3, F4, X", passVk, 0, 0);
    readKeyList("DisabledKeys", "Q, Shift", disabledVk, 0, 0);
    readKeyList("InstantKeys", "", instantVk, 0, 0);
    readKeyList("BackKeys", "Escape", backVk, 0, 0);
    {
        char sec[512]; DWORD n = GetPrivateProfileSectionA("Remap", sec, sizeof(sec), ini);
        const char *def = "Tab=S\0";
        const char *p = n ? sec : def;
        while (*p) { // "from=to" entries
            char from[32] = {0}, to[32] = {0}; unsigned i = 0;
            while (*p && *p != '=' && i < 31) from[i++] = *p++;
            if (*p == '=') p++;
            i = 0; while (*p && i < 31) to[i++] = *p++;
            p++;
            while (i && to[i - 1] == ' ') to[--i] = 0;
            unsigned f = 0; while (from[f]) f++; while (f && from[f - 1] == ' ') from[--f] = 0;
            const char *t = to; while (*t == ' ') t++;
            int a = keyCode(from), b = keyCode(t);
            if (a > 0 && b > 0) {
                remapTo[a] = (BYTE)b;
                if (remapCount < MAXREMAP) {
                    append(remapList[remapCount].from, sizeof(remapList[0].from), from);
                    append(remapList[remapCount].to, sizeof(remapList[0].to), t);
                    remapList[remapCount++].toVk = (BYTE)b;
                }
                if (remapText[0]) append(remapText, sizeof(remapText), ", ");
                append(remapText, sizeof(remapText), from); append(remapText, sizeof(remapText), " = "); append(remapText, sizeof(remapText), t);
            }
        }
    }
    readKeyList("TypeKey", "T", typeVk, typeText, sizeof(typeText));
    cfg.blockOtherKeys = GetPrivateProfileIntA("Options", "BlockOtherKeys", 1, ini);
    for (int i = 0; i < NBTN; i++) {
        readBinding("Buttons", btnName[i], btnDefaultKeys[i], &btn[i]);
        GetPrivateProfileStringA("Labels", btnName[i], btnDefaultLabel[i], btnLabel[i], sizeof(btnLabel[i]), ini);
    }
    cfg.typedChars = GetPrivateProfileIntA("Options", "TypedCharacters", 0, ini);
    cfg.accelMs = GetPrivateProfileIntA("Movement", "AccelMs", 45, ini);
    cfg.decelMs = GetPrivateProfileIntA("Movement", "DecelMs", 35, ini);
    cfg.turnMs = GetPrivateProfileIntA("Movement", "TurnMs", 110, ini);
    cfg.mouseWakePx = GetPrivateProfileIntA("Mouse", "MouseModeOnMovePx", 40, ini);
    // 0: moving the mouse never leaves controller mode; only the cursor key, menus
    // or the toggle key give you the mouse.
    cfg.mouseMoveSwitches = GetPrivateProfileIntA("Mouse", "MouseMoveSwitches", 1, ini);
    cfg.bumpNudgePct = GetPrivateProfileIntA("Mouse", "BumpNudgePct", 30, ini);
    cfg.clipCursor = GetPrivateProfileIntA("Mouse", "KeepCursorInWindow", 1, ini);
    cfg.cursorFromMiddle = GetPrivateProfileIntA("Mouse", "CursorFromMiddle", 0, ini);
    // Holding a mouse button bound to the bow switches to keyboard mode, so the
    // game aims the bow at the cursor.
    cfg.bowMouseAim = GetPrivateProfileIntA("Mouse", "BowAimsWithMouse", 1, ini);
    if (cfg.bowMouseAim) for (int k = 0; k < MAXKEYS; k++) if (btn[B_RT].vk[k] && btn[B_RT].vk[k] <= 0x06) bowVk[btn[B_RT].vk[k]] = 1;
    readButtonList("Keys", "MouseAfter", "DUp, DDown, Back, Start", mouseAfter);
    cfg.menuTapMs = GetPrivateProfileIntA("Keys", "MenuTapMs", 300, ini);
}

// "X, RT" -> flags per controller button.
static void readButtonList(const char *section, const char *name, const char *fallback, BYTE *flags) {
    char list[96], *p = list;
    GetPrivateProfileStringA(section, name, fallback, list, sizeof(list), ini);
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        char *start = p; while (*p && *p != ',') p++;
        char *end = p; while (end > start && end[-1] == ' ') end--;
        char save = *end; *end = 0;
        for (int i = 0; i < NBTN; i++) {
            const char *a = start, *b = btnName[i];
            while (*a && lower(*a) == lower(*b)) { a++; b++; }
            if (*start && !*a && !*b) flags[i] = 1; // case-insensitive match
        }
        *end = save;
    }
}

// Every key the mod owns; these are hidden from the game while it is active.
static BYTE owned[256];
static void markOwned(const Binding *b) { for (int i = 0; i < MAXKEYS; i++) if (b->vk[i]) owned[b->vk[i]] = 1; }
static void buildOwned(void) {
    markOwned(&moveUp); markOwned(&moveDown); markOwned(&moveLeft); markOwned(&moveRight); markOwned(&dodgeMouse);
    markOwned(&toggleKey); markOwned(&legendKey); markOwned(&cursorKey);
    for (int i = 0; i < NBTN; i++) markOwned(&btn[i]);
    if (owned[0x10]) owned[0xA0] = owned[0xA1] = 1; // Shift arrives as VK_SHIFT, L/R variants too
    if (owned[0x11]) owned[0xA2] = owned[0xA3] = 1;
    if (owned[0x12]) owned[0xA4] = owned[0xA5] = 1;
    // A key the layout uses is never also passed through or blocked (the editor never
    // writes both; this keeps a layout that leaves PassKeys/DisabledKeys out working).
    for (int vk = 0; vk < 256; vk++) if (owned[vk]) passVk[vk] = disabledVk[vk] = 0;
    for (int vk = 0; vk < 256; vk++) if (menuVk[vk]) owned[vk] = 0;
}

// ---------------------------------------------------------------- backend

static HANDLE tryBackend(const char *name) {
    HANDLE m = LoadLibraryA(name);
    if (m && m == self) { FreeLibrary(m); return 0; } // never recurse into ourselves
    return m;
}
// Wine and PCs with the old DirectX runtime have xinput1_3; stock Windows 10/11
// only has the system xinput1_4, loaded by full path so it isn't confused with
// this file. xinput9_1_0 is the last resort.
static HANDLE loadBackend(int systemOnly, const char **which) {
    HANDLE m;
    if (!systemOnly && (m = tryBackend("xinput1_3.dll"))) { *which = "xinput1_3"; return m; }
    char path[300]; UINT n = GetSystemDirectoryA(path, 260);
    if (n && n < 260) { path[n] = 0; append(path, sizeof(path), "\\xinput1_4.dll"); if ((m = tryBackend(path))) { *which = "system xinput1_4"; return m; } }
    if ((m = tryBackend("xinput9_1_0.dll"))) { *which = "xinput9_1_0"; return m; }
    return 0;
}

typedef struct { void **vt; } COM;
static int (*tfGetThreadMgr)(COM **);
static HANDLE (*immGetContext)(HANDLE);
static BOOL (*immReleaseContext)(HANDLE, HANDLE);
static void startLegend(void);

static int ready;
static void init(void) {
    if (ready) return;
    ready = 1;
    loadSettings();
    buildOwned();
    char backend[16];
    GetPrivateProfileStringA("Options", "Backend", "auto", backend, sizeof(backend), ini); // "system" tests the stock-Windows path
    const char *which = 0;
    real = loadBackend(named(backend, "system"), &which);
    HANDLE tf = LoadLibraryA("msctf.dll"), imm = LoadLibraryA("imm32.dll");
    if (tf) tfGetThreadMgr = GetProcAddress(tf, "TF_GetThreadMgr");
    if (imm) { immGetContext = GetProcAddress(imm, "ImmGetContext"); immReleaseContext = GetProcAddress(imm, "ImmReleaseContext"); }
    if (!immReleaseContext) immGetContext = 0;
    {   // A log over 1 MB starts over; the previous one is kept as wasdmod.old.log.
        char path[1100] = {0}, old[1100] = {0}; FILEINFO fi;
        append(path, sizeof(path), dir); append(path, sizeof(path), "wasdmod.log");
        append(old, sizeof(old), dir); append(old, sizeof(old), "wasdmod.old.log");
        if (GetFileAttributesExA(path, 0, &fi) && (fi.sizeHigh || fi.sizeLow > (1u << 20))) MoveFileExA(path, old, 1 /*REPLACE_EXISTING*/);
    }
    {   // Which build, on what: "=== 2026-10-03 wasdmod 1.0.0, Wine 9.0 ===".
        SYSTEMTIME t; GetLocalTime(&t);
        char head[200] = "=== ", d[11] = "0000-00-00";
        two(d, t.year / 100); two(d + 2, t.year); two(d + 5, t.month); two(d + 8, t.day);
        append(head, sizeof(head), d); append(head, sizeof(head), " wasdmod " VERSION ", ");
        HANDLE nt = GetModuleHandleA("ntdll.dll");
        const char *(*wine)(void) = nt ? GetProcAddress(nt, "wine_get_version") : 0;
        typedef struct { DWORD size, major, minor, build, platform; unsigned short csd[128]; } OSVER;
        long (*rtlVer)(OSVER *) = nt ? GetProcAddress(nt, "RtlGetVersion") : 0;
        OSVER v; memset(&v, 0, sizeof(v)); v.size = sizeof(v);
        if (wine) { append(head, sizeof(head), "Wine "); append(head, sizeof(head), wine()); }
        else if (rtlVer && !rtlVer(&v)) { append(head, sizeof(head), "Windows "); appendNum(head, sizeof(head), v.major); append(head, sizeof(head), "."); appendNum(head, sizeof(head), v.minor); append(head, sizeof(head), "."); appendNum(head, sizeof(head), v.build); }
        else append(head, sizeof(head), "Windows");
        append(head, sizeof(head), " ===");
        logline(head);
    }
    char line[160] = "Controller mod loaded; real controllers via ";
    append(line, sizeof(line), real ? which : "nothing (virtual pad only)");
    logline(line);
    { unsigned n = len(ini); const char *f = ini + n; while (f > ini && f[-1] != '\\') f--;
      char l2[128] = "Settings file: "; append(l2, sizeof(l2), f); logline(l2); }
    startLegend(); // overlay thread: key list, typing banner, the game window position, keeping the cursor inside
}

static void *realfn(const char *name) { init(); return real ? GetProcAddress(real, name) : 0; }

// ---------------------------------------------------------------- state

static volatile int typing, paused; // text box focused / mod switched off with the toggle key
static volatile int typingManual;     // typing mode (Enter), until Enter or Esc

// Keys the mod pressed for a remap (Tab -> S...) and hasn't released yet.
static BYTE remapSent[256];

// A key the mod itself holds down for a remap doesn't count as pressed: Tab sends
// the game's menu-wheel key S, which is also "move down".
static int held(const Binding *b) {
    for (int i = 0; i < MAXKEYS; i++) if (b->vk[i] && !remapSent[b->vk[i]] && (GetAsyncKeyState(b->vk[i]) & 0x8000)) return 1;
    return 0;
}

// Focus changes are rare and the checks cost wineserver round trips, so the
// answer is reused for 50 ms.
static int focused(void) {
    static U64 checkedAt; static int result;
    if (!cfg.requireFocus) return 1;
    U64 now = GetTickCount64();
    if (checkedAt && now - checkedAt < 50) return result;
    checkedAt = now;
    DWORD pid = 0; HANDLE w = GetForegroundWindow();
    if (w) GetWindowThreadProcessId(w, &pid);
    return result = w && pid == GetCurrentProcessId();
}

// Unreal tells Windows when a text box has focus: through Text Services it
// focuses that box's document manager instead of the one it keeps focused
// otherwise; through IMM it attaches an input context only while a box is
// active. While typing, the mod passes the keyboard through untouched.
static void *idleDocument;
static int detectMode;
static void setDetectMode(int mode) {
    if (detectMode == mode) return;
    detectMode = mode;
    logline(mode == 1 ? "Text box detection: Text Services" : "Text box detection: IMM");
}
static int textBoxFocused(void) {
    if (tfGetThreadMgr) {
        COM *tm = 0;
        if (tfGetThreadMgr(&tm) >= 0 && tm) {
            COM *doc = 0;
            ((int (*)(COM *, COM **))tm->vt[7])(tm, &doc); // ITfThreadMgr::GetFocus
            ((unsigned (*)(COM *))tm->vt[2])(tm);
            setDetectMode(1);
            static void *lastDoc; static int changes;
            if ((void *)doc != lastDoc) {
                lastDoc = doc;
                if (changes < 40) { changes++; logline(doc == 0 ? "Text Services focus: none" : doc == idleDocument || !idleDocument ? "Text Services focus: idle document" : "Text Services focus: another document (text box?)"); }
            }
            if (!doc) return 0;
            if (!idleDocument) idleDocument = doc; // the game starts without a text box focused
            int active = doc != idleDocument;
            ((unsigned (*)(COM *))doc->vt[2])(doc);
            return active;
        }
    }
    if (immGetContext) {
        HANDLE w = GetFocus();
        if (!w) { DWORD pid = 0; w = GetForegroundWindow(); if (!w || (GetWindowThreadProcessId(w, &pid), pid != GetCurrentProcessId())) return 0; }
        setDetectMode(2);
        HANDLE c = immGetContext(w);
        if (c) immReleaseContext(w, c);
        return c != 0;
    }
    return 0;
}

static volatile int legendVisible; static volatile U64 legendUntil;

// Mouse mode: the game gets the real mouse and keyboard (cursor, menu clicks)
// and the virtual pad rests. Entered while the Cursor key is held, or after a
// menu key (Tab/Esc) is released; left by pressing a movement key.
typedef struct { LONG x, y; } POINT;
IMP BOOL GetCursorPos(POINT *);

static volatile int mouseMenu, cursorHeld;
static volatile int cursorKeyMsg; // the cursor key is down, from its key messages
static volatile U64 nudgeUntil; // a small mouse bump in controller mode: nudge the right stick briefly
static volatile U64 moveGraceUntil; // the game is putting its cursor back in the middle: not the player's movement
static volatile U64 hookPressAt[256]; // when the input filter last handled a menu/back key press
static volatile int centerCursor;
static volatile int mouseMoveNudge; // the overlay thread wiggles the mouse by a pixel (cursor key pressed) // a menu opened from a key: overlay puts the cursor in the middle
static volatile BYTE bowDown[7]; // bow mouse buttons held, from press/release messages and state changes
static volatile int bowAiming; // a bow mouse button is held in keyboard mode: WASD doesn't switch back
static volatile int mouseFromMove; // mouse mode came from moving the mouse, not from opening a menu
// A menu or back key press (Esc is both). Mouse mode that came from moving the
// mouse has no menu open, so Esc then opens one instead of closing it.
// A bow mouse button pressed: keyboard mode, with no menu open (Esc opens one).
static void startBowAim(void) {
    if (!mouseMenu) { mouseMenu = 1; mouseFromMove = 1; }
    if (!bowAiming) logline("Bow held: keyboard mode, aim with the mouse");
    bowAiming = 1;
}
static void endBowAim(void) {
    for (int b = 1; b <= 6; b++) if (bowDown[b]) return;
    if (bowAiming) { bowAiming = 0; logline("Bow released (still mouse mode; WASD returns to controller)"); }
}
// Safety net: aiming only holds WASD back while a bow button really is held. If the
// release was missed, the next fight key ends aiming instead of being swallowed.
static int bowStillHeld(void) {
    for (int b = 1; b <= 6; b++) if (bowVk[b] && (GetAsyncKeyState(b) & 0x8000)) return 1;
    for (int b = 1; b <= 6; b++) bowDown[b] = 0;
    endBowAim();
    return 0;
}
static void menuKeyPress(int isMenu, int isBack, const char *toController, const char *toMouse) {
    if (mouseMenu && isBack && !mouseFromMove && !cursorHeld) { mouseMenu = 0; logline(toController); }
    else if (isMenu && (!mouseMenu || mouseFromMove)) { mouseMenu = 1; mouseFromMove = 0; centerCursor = 1; logline(toMouse); }
}
static int mouseMode(void) { return mouseMenu || cursorHeld; }

// ---------------------------------------------------------------- Record Logs
// The apps' Record Logs button puts wasdmod-record.flag next to the mod. While it's
// there, the log also records each key and mouse button the mod handles (and what
// it did with it) and the controller buttons it presses. Nothing is recorded while
// typing, and keys the layout doesn't use show only as "other key".
static volatile int recording;
static int commandHeld(void);
static void checkRecording(void) {
    static U64 checkedAt; U64 now = GetTickCount64();
    if (checkedAt && now - checkedAt < 1000) return;
    checkedAt = now;
    char flag[1100] = {0}; append(flag, sizeof(flag), dir); append(flag, sizeof(flag), "wasdmod-record.flag");
    int on = GetFileAttributesA(flag) != 0xFFFFFFFF;
    if (on && logBytes > (8u << 20)) { // a forgotten recording stops at 8 MB
        if (recording) logline("=== Recording stopped: the log is full. Click Stop & Save Logs. ===");
        recording = 0; return;
    }
    if (on == recording) return;
    recording = on;
    if (!on) { logline("=== Recording stopped ==="); return; }
    char l[200] = "=== Recording started (keys, buttons and modes below; nothing is recorded while typing) ===";
    logline(l);
    l[0] = 0; append(l, sizeof(l), "Now: ");
    append(l, sizeof(l), paused ? "wasdmod off (toggle key)" : typing || typingManual ? "typing" : mouseMode() ? "mouse mode" : "controller mode");
    { unsigned n = len(ini); const char *f = ini + n; while (f > ini && f[-1] != '\\') f--; append(l, sizeof(l), "; settings file "); append(l, sizeof(l), f); }
    logline(l);
}
// "W", "Tab", "Side 4 (back)"...
static void vkName(unsigned vk, char *out, unsigned cap) {
    static const struct { BYTE vk; const char *name; } names[] = {
        {0x01, "Left click"}, {0x02, "Right click"}, {0x04, "Middle click"}, {0x05, "Side 4 (back)"}, {0x06, "Side 5 (front)"},
        {0x08, "Backspace"}, {0x09, "Tab"}, {0x0D, "Enter"}, {0x10, "Shift"}, {0xA0, "Shift"}, {0xA1, "Right Shift"},
        {0x11, "Ctrl"}, {0xA2, "Ctrl"}, {0xA3, "Right Ctrl"}, {0x12, "Alt"}, {0xA4, "Alt"}, {0xA5, "Right Alt"},
        {0x14, "Caps Lock"}, {0x1B, "Esc"}, {0x20, "Space"}, {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
        {0xC0, "`"}, {0xBD, "-"}, {0xBB, "="}, {0xDB, "["}, {0xDD, "]"}, {0xBA, ";"}, {0xDE, "'"}, {0xBC, ","}, {0xBE, "."},
        {0xBF, "/"}, {0x5B, "Cmd/Windows"}, {0x5C, "Cmd/Windows"},
    };
    out[0] = 0;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { char c[2] = {(char)vk, 0}; append(out, cap, c); return; }
    if (vk >= 0x70 && vk <= 0x7B) { append(out, cap, "F"); appendNum(out, cap, vk - 0x6F); return; }
    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); i++) if (names[i].vk == vk) { append(out, cap, names[i].name); return; }
    append(out, cap, "key "); appendNum(out, cap, vk);
}
// Controller buttons, when they change: "Controller: A (Jump / interact), moving".
static void recordPad(const XINPUT_GAMEPAD *g) {
    static WORD lastButtons; static int lastLT, lastRT, lastMoving, lastRight, started;
    if (!recording) { started = 0; return; }
    int lt = g->bLeftTrigger > 30, rt = g->bRightTrigger > 30, moving = g->sThumbLX || g->sThumbLY, right = g->sThumbRX || g->sThumbRY;
    if (started && g->wButtons == lastButtons && lt == lastLT && rt == lastRT && moving == lastMoving && right == lastRight) return;
    started = 1; lastButtons = g->wButtons; lastLT = lt; lastRT = rt; lastMoving = moving; lastRight = right;
    char l[400] = "Controller: "; int any = 0;
    for (int i = 0; i < NBTN; i++) {
        int on = i == B_LT ? lt : i == B_RT ? rt : btnBit[i] && (g->wButtons & btnBit[i]);
        if (!on) continue;
        if (any++) append(l, sizeof(l), ", ");
        append(l, sizeof(l), btnName[i]); append(l, sizeof(l), " ("); append(l, sizeof(l), btnLabel[i]); append(l, sizeof(l), ")");
    }
    if (moving) { append(l, sizeof(l), any++ ? ", " : ""); append(l, sizeof(l), "moving (left stick)"); }
    if (right) { append(l, sizeof(l), any++ ? ", " : ""); append(l, sizeof(l), "right stick (dodge or nudge)"); }
    if (!any) append(l, sizeof(l), "nothing pressed");
    logline(l);
}


// The game window's client area in screen coordinates, kept current by the overlay thread.
static volatile LONG gameLeft, gameTop, gameWidth, gameHeight;
static void releaseRemaps(void) {
    for (int vk = 1; vk < 256; vk++) if (remapSent[vk]) { remapSent[vk] = 0; keybd_event((BYTE)vk, (BYTE)MapVirtualKeyA(vk, 0), 2 /*KEYEVENTF_KEYUP*/, 0); }
}
static int movementHeld(void) { return held(&moveUp) || held(&moveDown) || held(&moveLeft) || held(&moveRight); }

static void updateModes(void) {
    static int toggleWas, legendWas; static U64 checkedAt;
    checkRecording();
    int t = held(&toggleKey), l = held(&legendKey);
    if (focused()) {
        if (t && !toggleWas && !commandHeld()) { releaseRemaps(); paused = !paused; logline(paused ? "Controller keys off (toggle key)" : "Controller keys on (toggle key)"); }
        if (l && !legendWas) { legendVisible = !legendVisible; legendUntil = 0; }
        {   // Holding the cursor key (Option/Alt) pops the cursor into the middle of the
            // window; letting go hides it straight away, like Genshin: back to the
            // controller even when moving the mouse had brought the cursor out (an open
            // menu keeps it), with a right-stick nudge so the game hides its cursor
            // before anything moves it. In toggle mode a press shows it and the next
            // press locks it again.
            // From the key's own press/release only: CrossOver's key state can report
            // Alt as held forever when it's let go in another app.
            int c = cursorKeyMsg;
            if (cfg.cursorToggle) {
                static int keyWas, released;
                if (c && !keyWas && !commandHeld()) released = !released;
                keyWas = c;
                c = released;
            }
            if (c && !cursorHeld) {
                cursorHeld = 1; centerCursor = 1; logline("Cursor key held: cursor shown");
                // A tiny real mouse move, so the game switches to mouse & keyboard and shows its cursor.
                mouseMoveNudge = 1;
            }
            if (!c && cursorHeld) {
                if (mouseMenu && mouseFromMove && !bowAiming) mouseMenu = 0;
                if (!mouseMenu) { U64 now = GetTickCount64(); nudgeUntil = now + 40; moveGraceUntil = now + 400; }
                logline(mouseMenu ? "Cursor key released (menu open: still mouse mode)" : "Cursor key released: controller mode");
            }
            cursorHeld = c;
        }
        // Normally the input filter switches modes as the key arrives; this catches misses.
        // A press the filter never saw (it handles it within a few ms) is applied here.
        if (!paused && !typing && !typingManual) {
            static BYTE was[256]; static U64 pending[256];
            U64 tick = GetTickCount64();
            for (int vk = 1; vk < 256; vk++) {
                if (!menuVk[vk] && !backVk[vk]) continue;
                int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
                if (down && !was[vk]) pending[vk] = tick;
                was[vk] = (BYTE)down;
                if (!pending[vk] || tick - pending[vk] < 150) continue;
                if (hookPressAt[vk] + 300 < pending[vk]) {
                    menuKeyPress(menuVk[vk], backVk[vk], "Controller mode (back key, missed message)", "Mouse mode (menu key, missed message)");
                }
                pending[vk] = 0;
            }
        }
        if (cfg.bowMouseAim && !paused && !typing && !typingManual) {
            // Only changes count: Wine can report a side button as held when it isn't.
            static BYTE was[7]; static int primed; int bow = 0;
            for (int vk = 1; vk <= 6; vk++) {
                if (!bowVk[vk]) continue;
                int d = (GetAsyncKeyState(vk) & 0x8000) != 0;
                if (d && !was[vk] && primed && focused() && !commandHeld()) bowDown[vk] = 1;
                if (!d && was[vk]) bowDown[vk] = 0;
                was[vk] = (BYTE)d;
                if (bowDown[vk]) bow = 1;
            }
            primed = 1;
            if (bow && !bowAiming) startBowAim();
            if (!bow) endBowAim();
        } else bowAiming = 0;
        if (mouseMenu && movementHeld() && !(bowAiming && bowStillHeld())) { mouseMenu = 0; logline("Controller mode (movement key)"); }
        // A menu key held longer than a tap was a quick overlay (hold Tab/I = mini
        // inventory, hold G = emote wheel): back to controller mode when it's let go.
        {
            static int menuWas; static U64 menuSince;
            int menuNow = 0;
            for (int vk = 1; vk < 256 && !menuNow; vk++) if (menuVk[vk] && (GetAsyncKeyState(vk) & 0x8000)) menuNow = 1;
            U64 tick = GetTickCount64();
            if (menuNow && !menuWas) menuSince = tick;
            if (!menuNow && menuWas && mouseMenu && tick - menuSince >= (U64)cfg.menuTapMs) { mouseMenu = 0; logline("Controller mode (menu key held, overlay closed)"); }
            menuWas = menuNow;
        }
    } else { cursorHeld = cursorKeyMsg = 0; releaseRemaps(); } // a release in another app never arrives: start over
    toggleWas = t; legendWas = l;
    // Moving the mouse while not moving the character switches to mouse mode (the
    // cursor stays where it is). While a movement key or mouse button is held the
    // reference point just follows, so nudges during a fight don't count.
    // With MouseMoveSwitches=0 it never switches. Either way a small bump flips the
    // game's prompts to keyboard; once the mouse has stopped, one short right-stick
    // nudge flips it back. Never while the mouse is moving: going back to the
    // controller, the game puts its cursor in the middle, so nudging a moving mouse
    // traps it there. That jump back to the middle isn't the player's movement.
    {
        static int haveRef; static POINT ref;
        int busy = movementHeld() || ((GetAsyncKeyState(0x01) | GetAsyncKeyState(0x02) | GetAsyncKeyState(0x04) | GetAsyncKeyState(0x05) | GetAsyncKeyState(0x06)) & 0x8000);
        int watch = cfg.mouseMoveSwitches ? cfg.mouseWakePx : cfg.bumpNudgePct;
        POINT p;
        int gotPos = GetCursorPos(&p);
        if (!watch || mouseMode() || typing || typingManual || paused || !focused() || !gotPos) haveRef = 0;
        else if (busy || !haveRef) { ref = p; haveRef = 1; }
        else {
            static POINT last; static int haveLast; static U64 movedAt;
            U64 t = GetTickCount64();
            if (t < moveGraceUntil) { ref = p; last = p; haveLast = 1; movedAt = 0; } // the game re-centring its cursor
            else {
                LONG dx = p.x - ref.x, dy = p.y - ref.y;
                int moved = haveLast && (p.x != last.x || p.y != last.y);
                if (cfg.mouseMoveSwitches && dx * dx + dy * dy > (LONG)cfg.mouseWakePx * cfg.mouseWakePx) {
                    mouseMenu = 1; mouseFromMove = 1; haveRef = 0; haveLast = 0; movedAt = 0; logline("Mouse mode (mouse moved)");
                    if (cfg.cursorFromMiddle) centerCursor = 1; // the cursor comes out in the middle, not where the mouse drifted
                } else if (cfg.bumpNudgePct) {
                    if (moved) movedAt = t;
                    else if (movedAt && t - movedAt >= 150) { nudgeUntil = t + 40; movedAt = 0; moveGraceUntil = t + 400; }
                }
                last = p; haveLast = 1;
            }
        }
    }
    U64 tick = GetTickCount64();
    if (!cfg.detectText || (checkedAt && tick - checkedAt < 50)) return;
    checkedAt = tick;
    int now = textBoxFocused();
    if (now != typing) { typing = now; logline(now ? "Text box focused: keyboard passes through" : "Text box closed: controller keys on"); }
}

static int commandHeld(void) { return (GetAsyncKeyState(0x5B) & 0x8000) || (GetAsyncKeyState(0x5C) & 0x8000); }


static int active(void) { return !typing && !typingManual && !paused && !mouseMode() && focused(); }

// The game window's client area in screen coordinates, kept current by the
// overlay thread (legend.inc). Used as the aiming centre.
IMP BOOL SetCursorPos(int, int);
// Controller mode: the game hides its cursor, so keep that invisible cursor inside
// the game window (a few pixels in from the edges) -- a click can't land in another
// app, and it can't get lost. Checked every poll; mouse mode leaves it free.
static void keepCursorInside(void) {
    LONG l = gameLeft, t = gameTop, w = gameWidth, h = gameHeight;
    if (!cfg.clipCursor || w < 400 || h < 300) return;
    POINT p; if (!GetCursorPos(&p)) return;
    LONG x = p.x < l + 4 ? l + 4 : p.x > l + w - 5 ? l + w - 5 : p.x;
    LONG y = p.y < t + 4 ? t + 4 : p.y > t + h - 5 ? t + h - 5 : p.y;
    if (x != p.x || y != p.y) SetCursorPos(x, y);
}

static Smooth moveSmooth;
static float lastDirX, lastDirY = 1; // last direction of travel, for dodging
static long long lastPollTime;

static DodgeState mouseDodge;

static SHORT stickValue(float v) { v *= 32767; if (v > 32767) v = 32767; if (v < -32767) v = -32767; return (SHORT)v; }


// Builds the virtual pad from the keyboard. Returns 1 if anything is pressed.
static int keyboardPad(XINPUT_GAMEPAD *g) {
    memset(g, 0, sizeof(*g));
    long long now; QueryPerformanceCounter(&now);
    float dt = lastPollTime && perfFreq ? (float)(now - lastPollTime) * 1000.0f / (float)perfFreq : 16.7f;
    lastPollTime = now;
    if (!active() || commandHeld()) { moveSmooth.mag = 0; memset(&mouseDodge, 0, sizeof(mouseDodge)); return 0; }
    U64 tick = GetTickCount64();
    int any = 0;
    for (int i = 0; i < NBTN; i++) {
        int down = held(&btn[i]), press = down;
        if (i == B_RT && cfg.bowMouseAim) { down = 0; for (int k = 0; k < MAXKEYS; k++) if (btn[i].vk[k] && !bowVk[btn[i].vk[k]] && (GetAsyncKeyState(btn[i].vk[k]) & 0x8000)) down = 1; press = down; }
        if (mouseAfter[i]) {
            // A tap opens the full menu -> mouse mode on release. A long hold is a
            // quick overlay (e.g. the mini inventory) that closes on release, so the
            // mod stays in controller mode.
            static BYTE wasDown[NBTN]; static U64 downSince[NBTN];
            if (down && !wasDown[i]) downSince[i] = tick;
            if (wasDown[i] && !down) {
                if (suppressMouseAfter) suppressMouseAfter = 0; // that press closed the menu
                else if (tick - downSince[i] < (U64)cfg.menuTapMs) { mouseMenu = 1; mouseFromMove = 0; centerCursor = 1; logline("Mouse mode (menu button)"); }
            }
            wasDown[i] = (BYTE)down;
        }
        if (!press) continue;
        any = 1;
        if (i == B_LT) g->bLeftTrigger = 255;
        else if (i == B_RT) g->bRightTrigger = 255;
        else g->wButtons |= btnBit[i];
    }
    // Movement: 8-way keys, smoothed.
    int kx = held(&moveRight) - held(&moveLeft), ky = held(&moveUp) - held(&moveDown);
    float tx = (float)kx, ty = (float)ky;
    if (kx && ky) { tx *= 0.70710678f; ty *= 0.70710678f; }
    SmoothCfg sc = {(float)cfg.accelMs, (float)cfg.decelMs, (float)cfg.turnMs};
    float mx, my;
    smoothStep(&moveSmooth, &sc, tx, ty, dt, &mx, &my);
    if (kx || ky) { lastDirX = tx; lastDirY = ty; any = 1; }
    g->sThumbLX = stickValue(mx); g->sThumbLY = stickValue(my);
    if (mx != 0 || my != 0) any = 1;
    // A small mouse bump flips the game to keyboard mode; a brief, light right-stick
    // nudge (well below a dodge flick) flips it straight back.
    if (tick < nudgeUntil && !g->sThumbRX && !g->sThumbRY) { any = 1; g->sThumbRX = stickValue(cfg.bumpNudgePct / 100.0f); }
    // Mouse dodge (dodge.inc): the cursor is only read while the button is in use.
    if (dodgeMouse.vk[0]) {
        float fx, fy; POINT p = {0, 0}; int cursorOk = 0;
        int down = held(&dodgeMouse);
        if (down || mouseDodge.down) cursorOk = GetCursorPos(&p);
        DodgeCfg dc = {(unsigned)cfg.dodgeDragPx, (unsigned)cfg.dodgeFlickMs};
        dodgeStep(&mouseDodge, &dc, down, tick, lastDirX, lastDirY, cursorOk, p.x, p.y, &fx, &fy);
        if (fx != 0 || fy != 0) { any = 1; g->sThumbRX = stickValue(fx); g->sThumbRY = stickValue(fy); }
    }
    return any;
}

// ---------------------------------------------------------------- hiding input from the game
//
// A message hook on the game's window threads turns the mod's keys and mouse
// input into WM_NULL before the game reads them, so the game never sees keyboard
// or mouse input and stays in controller mode. It runs for every message
// (including high-rate mouse input), so it makes no system calls except
// TranslateMessage for owned key presses: that keeps their typed characters, so
// letters still reach a text box if detection ever misses one.

typedef struct { HANDLE hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; long x, y; } MSG;

static int mouseVk(UINT msg, WPARAM wParam) {
    if (msg >= 0x201 && msg <= 0x203) return 0x01;
    if (msg >= 0x204 && msg <= 0x206) return 0x02;
    if (msg >= 0x207 && msg <= 0x209) return 0x04;
    if (msg >= 0x20B && msg <= 0x20D) return ((wParam >> 16) & 0xFFFF) == 1 ? 0x05 : 0x06;
    return 0;
}

IMP UINT GetRawInputData(HANDLE, UINT, void *, UINT *, UINT);

// Raw input: the game reads the keyboard this way too (not only WM_KEYDOWN),
// so raw keyboard events for the mod's keys must be hidden as well. Returns the
// key's virtual-key code, 0 for mouse or other devices. Timed for the log.
static long long rawTime; static unsigned rawCalls;
static unsigned rawKeyboardVk(LPARAM handle, int *isDown) {
    long long t0 = 0, t1; if (perfFreq) QueryPerformanceCounter(&t0);
    BYTE raw[64]; UINT size = sizeof(raw); unsigned vk = 0;
    // RAWINPUT: header {dwType, dwSize, hDevice, wParam} = 24 bytes, then RAWKEYBOARD
    // {MakeCode, Flags, Reserved, VKey, Message, ExtraInformation}.
    if (GetRawInputData((HANDLE)handle, 0x10000003 /*RID_INPUT*/, raw, &size, 24) != (UINT)-1 && size >= 36 && *(DWORD *)raw == 1 /*RIM_TYPEKEYBOARD*/) {
        vk = *(WORD *)(raw + 30);
        *isDown = !(*(WORD *)(raw + 26) & 1 /*RI_KEY_BREAK*/);
    }
    if (t0) { QueryPerformanceCounter(&t1); rawTime += t1 - t0; rawCalls++; }
    return vk & 0xFF;
}

// A "fight key": any keyboard key the mod turns into controller input, except
// its own utility keys. Pressing one in mouse mode returns to controller mode.
static int isFightVk(unsigned vk) {
    if (!owned[vk] || vk <= 0x06) return 0; // mouse buttons never leave mouse mode
    const Binding *utility[3] = {&toggleKey, &legendKey, &cursorKey};
    for (int b = 0; b < 3; b++) for (int k = 0; k < MAXKEYS; k++) if (utility[b]->vk[k] == vk) return 0;
    return 1;
}

static LRESULT filterInput(int code, WPARAM wParam, LPARAM lParam) {
    MSG *m = (MSG *)lParam;
    if (code != 0 || !m) return 0;
    UINT msg = m->message;
    // Typing mode: the type key (T) starts it and is swallowed, so it doesn't
    // type itself. While typing every key reaches the game (T and Enter included);
    // Esc ends it and is kept from the game (so it can't open the pause menu).
    if (msg == 0x100 || msg == 0x104) {
        unsigned tk = m->wParam & 0xFF; int repeat = (m->lParam >> 30) & 1;
        if (!typingManual && typeVk[tk] && !paused && !typing) {
            if (!repeat && wParam == 1 /*PM_REMOVE*/) { typingManual = 1; logline("Typing mode on (type key)"); }
            m->message = 0;
            return 0;
        }
        if (typingManual && tk == 0x1B) { // Esc ends typing and is kept from the game
            if (wParam == 1) { typingManual = 0; logline("Typing mode off (Esc)"); }
            m->message = 0;
            return 0;
        }
    }
    // The raw-input copy of that Esc arrives first; keep it from the game too.
    if (msg == 0xFF && typingManual) {
        int down; if (rawKeyboardVk(m->lParam, &down) == 0x1B) m->message = 0;
        return 0;
    }
    if (typing || typingManual || paused) return 0;
    // Command (Wine maps it to the Windows keys) held: no key registers, so Mac
    // shortcuts like Cmd+Tab / Cmd+Q never move you or fire an ability.
    if ((msg == 0x100 || msg == 0x101 || msg == 0x104 || msg == 0x105) && commandHeld()) { m->message = 0; return 0; }
    // Remapped keys: hide the original (and its raw copy) and press the target key.
    if (msg == 0x100 || msg == 0x101 || msg == 0x104 || msg == 0x105) {
        unsigned rk = m->wParam & 0xFF; BYTE to = remapTo[rk];
        if (to) {
            int isDown = msg == 0x100 || msg == 0x104, repeat = (m->lParam >> 30) & 1;
            // The key goes down only while the game is in front, and always comes back up
            // once it went down (a key left down stays "held" for the whole bottle).
            if (wParam == 1 /*PM_REMOVE*/ && (!isDown || !repeat) && (isDown ? focused() : remapSent[to])) {
                sentUntil[to] = GetTickCount64() + 500;
                remapSent[to] = (BYTE)isDown;
                keybd_event(to, (BYTE)MapVirtualKeyA(to, 0), isDown ? 0 : 2 /*KEYEVENTF_KEYUP*/, 0);
                // A remap onto one of the PassKeys is an instant action (teleport...), not a menu.
                if (isDown && !passVk[to] && !instantVk[to]) menuKeyPress(1, backVk[to], "Controller mode (remapped back key)", "Mouse mode (remapped menu key)");
            }
            m->message = 0;
            return 0;
        }
    }
    if (msg == 0xFF) {
        int rd; unsigned rk = rawKeyboardVk(m->lParam, &rd);
        if (rk && (remapTo[rk] || commandHeld())) { m->message = 0; return 0; }
    }
    unsigned vk = 0; int down = 0, raw = 0;
    if (msg == 0x100 || msg == 0x101 || msg == 0x104 || msg == 0x105) { // WM_(SYS)KEYDOWN/UP
        vk = m->wParam & 0xFF; down = msg == 0x100 || msg == 0x104;
    } else if (msg == 0xFF) { // WM_INPUT: raw keyboard is handled like keys, raw mouse like the mouse
        vk = rawKeyboardVk(m->lParam, &down); raw = 1;
        if (!vk) {
            if (cfg.hideMouse && !mouseMode()) m->message = 0;
            return 0;
        }
    } else if (msg >= 0x200 && msg <= 0x20E) { // mouse messages: clicks never change modes (except the bow)
        int mvk = mouseVk(msg, m->wParam);
        if (mvk && disabledVk[mvk]) { m->message = 0; return 0; } // the game's button for an action you moved
        if (mvk && bowVk[mvk]) {
            if (msg == 0x20B || msg == 0x201 || msg == 0x204 || msg == 0x207) { bowDown[mvk] = 1; startBowAim(); }
            else if (msg == 0x20C || msg == 0x202 || msg == 0x205 || msg == 0x208) { bowDown[mvk] = 0; endBowAim(); }
        }
        if (mouseMode()) {
            // Side-button remaps (Mouse5=Mouse4, Mouse4=Mouse2...): the game sees the other button.
            BYTE to = mvk >= 0x05 ? remapTo[mvk] : 0;
            if (to >= 0x01 && to <= 0x06 && to != 0x03 && msg >= 0x20B && msg <= 0x20D) {
                static const UINT downMsg[7] = {0, 0x201, 0x204, 0, 0x207, 0x20B, 0x20B};
                static const WPARAM flag[7] = {0, 0x01, 0x02, 0, 0x10, 0x20, 0x40};
                WPARAM flags = m->wParam & 0xFFFF;
                if (flags & flag[mvk]) flags = (flags & ~flag[mvk]) | flag[to];
                m->message = downMsg[to] + (msg - 0x20B); // down / up / double click
                m->wParam = to >= 0x05 ? ((WPARAM)(to == 0x05 ? 1 : 2) << 16) | flags : flags;
            }
            return 0;
        }
        if (cfg.hideMouse || (mvk && owned[mvk])) m->message = 0;
        return 0;
    } else return 0;

    if (passVk[vk]) return 0;
    if (sentUntil[vk] && GetTickCount64() < sentUntil[vk]) return 0; // sent by the mod (remap)
    // The game's own key for an action you put on another key stays off in every mode
    // (typing mode and text boxes aside, handled above).
    if (disabledVk[vk]) { m->message = 0; return 0; }
    // The game's menu shortcuts always reach the game. The normal key message
    // (not its raw copy, which arrives first) decides the mode, once per press:
    // in controller mode a menu key opens mouse mode; in mouse mode a back key
    // (Esc) returns to controller mode.
    // The hold-for-cursor keys (Alt/Ctrl) are the mod's own: never passed on, so a
    // lone Alt can't open the window menu or count as a keyboard press.
    for (int k = 0; k < MAXKEYS; k++) {
        BYTE c = cursorKey.vk[k];
        if (c && (c == vk || (c == 0x12 && (vk == 0xA4 || vk == 0xA5)) || (c == 0x11 && (vk == 0xA2 || vk == 0xA3)))) {
            if (!raw && wParam == 1 /*PM_REMOVE*/) cursorKeyMsg = down;
            m->message = 0; return 0;
        }
    }
    // Esc and the menu shortcuts always reach the game, pressed and released, in
    // every mode (only typing mode keeps Esc for itself).
    if (menuVk[vk] || backVk[vk]) {
        int repeat = !raw && ((m->lParam >> 30) & 1);
        // Mode changes happen once per press: on the normal message (not its raw
        // copy), and only when it's taken off the queue (a peek would count twice).
        if (!raw && down && !repeat && wParam == 1 /*PM_REMOVE*/) {
            hookPressAt[vk] = GetTickCount64();
            menuKeyPress(menuVk[vk], backVk[vk], "Controller mode (back key)", "Mouse mode (menu key)");
        }
        return 0;
    }
    if (mouseMode()) {
        // Everything reaches the game, except fight keys (movement, jump,
        // abilities...), which switch straight back to controller mode.
        if (!isFightVk(vk)) return 0;
        if (bowAiming && bowStillHeld()) { m->message = 0; return 0; } // aiming: stay in keyboard mode
        if (down && mouseMenu && !cursorHeld) {
            mouseMenu = 0; logline("Controller mode (fight key)");
            for (int i = 0; i < NBTN; i++) if (mouseAfter[i]) for (int k = 0; k < MAXKEYS; k++) if (btn[i].vk[k] == vk) suppressMouseAfter = 1;
        }
        if (!mouseMode()) m->message = 0;
        return 0;
    }
    if (!owned[vk] && !cfg.blockOtherKeys) return 0;
    // Optional: keep the typed character for text boxes the detection misses.
    if (!raw && cfg.typedChars && down && wParam == 1 /*PM_REMOVE*/) { MSG copy = *m; TranslateMessage(&copy); }
    m->message = 0;
    return 0;
}

// Record Logs: what the filter did with a key or mouse button press.
static int layoutKey(unsigned vk) {
    if (vk <= 0x06 || owned[vk] || menuVk[vk] || passVk[vk] || disabledVk[vk] || remapTo[vk] || backVk[vk] || typeVk[vk]) return 1;
    for (int k = 0; k < 256; k++) if (remapTo[k] == vk) return 1;
    return 0;
}
static void recordInput(UINT msg, WPARAM wp, LPARAM lp, UINT after, WPARAM afterWp) {
    int key = msg == 0x100 || msg == 0x101 || msg == 0x104 || msg == 0x105;
    int btn = msg == 0x201 || msg == 0x202 || msg == 0x204 || msg == 0x205 || msg == 0x207 || msg == 0x208 || msg == 0x20B || msg == 0x20C;
    if ((!key && !btn) || typing || typingManual) return;
    int down = key ? msg == 0x100 || msg == 0x104 : msg == 0x201 || msg == 0x204 || msg == 0x207 || msg == 0x20B;
    if (key && down && ((lp >> 30) & 1)) return; // auto-repeat
    unsigned vk = key ? (unsigned)(wp & 0xFF) : (unsigned)mouseVk(msg, wp);
    char l[200] = {0}, name[32];
    if (layoutKey(vk)) vkName(vk, name, sizeof(name)); else { name[0] = 0; append(name, sizeof(name), "other key"); }
    append(l, sizeof(l), name); append(l, sizeof(l), down ? " down: " : " up: ");
    if (paused) append(l, sizeof(l), "passed to the game (wasdmod is off)");
    else if (key && sentUntil[vk] && GetTickCount64() < sentUntil[vk]) append(l, sizeof(l), "passed to the game (sent by wasdmod for a conversion)");
    else if (!after) {
        if (key && remapTo[vk]) { append(l, sizeof(l), "converted: the game gets "); vkName(remapTo[vk], name, sizeof(name)); append(l, sizeof(l), name); }
        else if (disabledVk[vk]) append(l, sizeof(l), "blocked (the game's own key for an action you moved)");
        else if (commandHeld()) append(l, sizeof(l), "ignored (Cmd/Windows key held)");
        else append(l, sizeof(l), "kept from the game (wasdmod uses it)");
    } else if (btn && (after != msg || afterWp != wp)) { append(l, sizeof(l), "passed to the game as "); vkName(mouseVk(after, afterWp), name, sizeof(name)); append(l, sizeof(l), name); }
    else append(l, sizeof(l), "passed to the game");
    append(l, sizeof(l), bowAiming ? " [aiming the bow]" : mouseMode() ? " [mouse mode]" : " [controller mode]");
    logline(l);
}
static LRESULT inputFilter(int code, WPARAM wParam, LPARAM lParam) {
    MSG *m = (MSG *)lParam;
    if (!recording || code != 0 || !m || wParam != 1 /*PM_REMOVE*/) return filterInput(code, wParam, lParam);
    UINT msg = m->message; WPARAM wp = m->wParam; LPARAM lp = m->lParam;
    LRESULT r = filterInput(code, wParam, lParam);
    recordInput(msg, wp, lp, m->message, m->wParam);
    return r;
}

static DWORD hooked[16];
static int hookedCount;
static BOOL hookWindowThread(HANDLE w, LPARAM unused) {
    DWORD pid = 0, tid = GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || !tid) return 1;
    for (int i = 0; i < hookedCount; i++) if (hooked[i] == tid) return 1;
    if (hookedCount >= 16) return 0;
    hooked[hookedCount++] = tid;
    HANDLE h = SetWindowsHookExW(3 /*WH_GETMESSAGE*/, inputFilter, 0, tid);
    logline(h ? "Keyboard/mouse hidden from the game on a window thread" : "Could not hook a window thread");
    return 1;
}
// Rescans every ~2 seconds of polling to catch windows created after startup.
static void ensureFilter(void) {
    static unsigned polls;
    if (polls++ % 240) return;
    EnumWindows(hookWindowThread, 0);
}

// ---------------------------------------------------------------- timing

static long long spentTotal, spentMax, statsStart; static unsigned statsCalls;
static void recordTime(long long start) {
    long long now; QueryPerformanceCounter(&now);
    long long d = now - start; spentTotal += d; if (d > spentMax) spentMax = d; statsCalls++;
    if (!statsStart) statsStart = start;
    if (perfFreq && now - statsStart > perfFreq * 30) { // every 30 s
        char line[160] = "Poll cost over 30 s: avg ";
        appendNum(line, sizeof(line), statsCalls ? (U64)(spentTotal * 1000000 / perfFreq / statsCalls) : 0);
        append(line, sizeof(line), " us, max ");
        appendNum(line, sizeof(line), (U64)(spentMax * 1000000 / perfFreq));
        append(line, sizeof(line), " us, polls ");
        appendNum(line, sizeof(line), statsCalls);
        append(line, sizeof(line), "; raw input checks ");
        appendNum(line, sizeof(line), rawCalls);
        append(line, sizeof(line), " avg ");
        appendNum(line, sizeof(line), rawCalls ? (U64)(rawTime * 1000000 / perfFreq / rawCalls) : 0);
        append(line, sizeof(line), " us");
        logline(line);
        spentTotal = spentMax = 0; statsCalls = 0; statsStart = now; rawTime = 0; rawCalls = 0;
    }
}

// ---------------------------------------------------------------- XInput

typedef DWORD (*GetStateFn)(DWORD, XINPUT_STATE *);
static DWORD packet;
static XINPUT_GAMEPAD lastPad;

static DWORD getState(const char *name, DWORD index, XINPUT_STATE *state) {
    init();
    if (!state) return 87; // ERROR_INVALID_PARAMETER
    GetStateFn fn = (GetStateFn)realfn(name);
    DWORD r = fn ? fn(index, state) : ERROR_DEVICE_NOT_CONNECTED;
    if (index != 0) return r;
    long long start = 0; if (perfFreq) QueryPerformanceCounter(&start);
    ensureFilter();
    updateModes();
    XINPUT_GAMEPAD k;
    int keys = keyboardPad(&k);
    if (r == ERROR_SUCCESS) { // real controller: merge, keyboard sticks win while held
        XINPUT_GAMEPAD *g = &state->Gamepad;
        g->wButtons |= k.wButtons;
        if (k.bLeftTrigger > g->bLeftTrigger) g->bLeftTrigger = k.bLeftTrigger;
        if (k.bRightTrigger > g->bRightTrigger) g->bRightTrigger = k.bRightTrigger;
        if (k.sThumbLX || k.sThumbLY) { g->sThumbLX = k.sThumbLX; g->sThumbLY = k.sThumbLY; }
        if (k.sThumbRX || k.sThumbRY) { g->sThumbRX = k.sThumbRX; g->sThumbRY = k.sThumbRY; }
    } else if (keys || cfg.alwaysConnected) {
        memset(state, 0, sizeof(*state));
        state->Gamepad = k;
        r = ERROR_SUCCESS;
    }
    // Packet number changes whenever the reported pad changes.
    XINPUT_GAMEPAD *g = &state->Gamepad; const BYTE *a = (const BYTE *)g, *b = (const BYTE *)&lastPad; int changed = 0;
    for (unsigned i = 0; i < sizeof(*g); i++) if (a[i] != b[i]) changed = 1;
    if (changed) { packet++; lastPad = *g; }
    if (r == ERROR_SUCCESS && state->dwPacketNumber < packet) state->dwPacketNumber = packet;
    if (r == ERROR_SUCCESS) recordPad(&state->Gamepad);
    if (start) recordTime(start);
    return r;
}

static int virtualPad(DWORD index) {
    if (index != 0 || !cfg.alwaysConnected) return 0;
    XINPUT_STATE s; GetStateFn fn = (GetStateFn)realfn("XInputGetState");
    return !fn || fn(0, &s) != ERROR_SUCCESS;
}

EXP DWORD XInputGetState(DWORD index, XINPUT_STATE *state) { return getState("XInputGetState", index, state); }

EXP DWORD XInputGetStateEx(DWORD index, XINPUT_STATE *state) {
    init();
    return getState(real && GetProcAddress(real, (const char *)100) ? (const char *)100 : "XInputGetState", index, state);
}

EXP DWORD XInputGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES *caps) {
    DWORD (*fn)(DWORD, DWORD, XINPUT_CAPABILITIES *) = realfn("XInputGetCapabilities");
    if (!virtualPad(index)) return fn ? fn(index, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
    if (!caps) return 87;
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1; caps->SubType = 1; // XINPUT_DEVTYPE_GAMEPAD / XINPUT_DEVSUBTYPE_GAMEPAD
    caps->Gamepad.wButtons = 0xF3FF;
    caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 0xFF;
    caps->Gamepad.sThumbLX = caps->Gamepad.sThumbLY = caps->Gamepad.sThumbRX = caps->Gamepad.sThumbRY = (SHORT)0xffc0;
    return ERROR_SUCCESS;
}

EXP DWORD XInputSetState(DWORD index, XINPUT_VIBRATION *vibration) {
    DWORD (*fn)(DWORD, XINPUT_VIBRATION *) = realfn("XInputSetState");
    if (virtualPad(index)) return ERROR_SUCCESS;
    return fn ? fn(index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}

EXP void XInputEnable(BOOL enable) {
    void (*fn)(BOOL) = realfn("XInputEnable");
    if (fn) fn(enable);
}

EXP DWORD XInputGetBatteryInformation(DWORD index, BYTE type, XINPUT_BATTERY_INFORMATION *info) {
    DWORD (*fn)(DWORD, BYTE, XINPUT_BATTERY_INFORMATION *) = realfn("XInputGetBatteryInformation");
    if (virtualPad(index) && info) { info->BatteryType = 1; info->BatteryLevel = 3; return ERROR_SUCCESS; } // wired, full
    return fn ? fn(index, type, info) : ERROR_DEVICE_NOT_CONNECTED;
}

EXP DWORD XInputGetKeystroke(DWORD index, DWORD reserved, void *keystroke) {
    DWORD (*fn)(DWORD, DWORD, void *) = realfn("XInputGetKeystroke");
    if (virtualPad(index)) return ERROR_EMPTY;
    return fn ? fn(index, reserved, keystroke) : ERROR_DEVICE_NOT_CONNECTED;
}

EXP DWORD XInputGetAudioDeviceIds(DWORD index, WCHAR *render, UINT *renderCount, WCHAR *capture, UINT *captureCount) {
    DWORD (*fn)(DWORD, WCHAR *, UINT *, WCHAR *, UINT *) = realfn("XInputGetAudioDeviceIds");
    return fn ? fn(index, render, renderCount, capture, captureCount) : ERROR_DEVICE_NOT_CONNECTED;
}

// Undocumented ordinal exports: forwarded unchanged when available.
EXP DWORD XInputWaitForGuideButton(DWORD index, DWORD flags, void *listen) {
    DWORD (*fn)(DWORD, DWORD, void *) = realfn((const char *)101);
    return fn ? fn(index, flags, listen) : ERROR_DEVICE_NOT_CONNECTED;
}
EXP DWORD XInputCancelGuideButtonWait(DWORD index) {
    DWORD (*fn)(DWORD) = realfn((const char *)102);
    return fn ? fn(index) : ERROR_DEVICE_NOT_CONNECTED;
}
EXP DWORD XInputPowerOffController(DWORD index) {
    DWORD (*fn)(DWORD) = realfn((const char *)103);
    return fn ? fn(index) : ERROR_DEVICE_NOT_CONNECTED;
}
EXP DWORD XInputGetBaseBusInformation(DWORD index, void *info) {
    DWORD (*fn)(DWORD, void *) = realfn((const char *)104);
    return fn ? fn(index, info) : ERROR_DEVICE_NOT_CONNECTED;
}
EXP DWORD XInputGetCapabilitiesEx(DWORD unknown, DWORD index, DWORD flags, void *caps) {
    DWORD (*fn)(DWORD, DWORD, DWORD, void *) = realfn((const char *)108);
    return fn ? fn(unknown, index, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
}

#include "legend.inc"

BOOL DllMain(HANDLE module, DWORD reason, void *reserved) {
    if (reason == 1) { self = module; QueryPerformanceFrequency(&perfFreq); }
    return 1;
}
