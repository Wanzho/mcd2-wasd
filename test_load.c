// Loads the mod DLL from its own folder and checks the exports behave:
// controller 0 appears connected, the stick is centred with no keys held,
// and the builtin XInput backend is reachable.
// test_load.exe --overlay [dark] shows the on-screen overlays instead (see overlayTest).
typedef unsigned long DWORD;
typedef void *HANDLE;
typedef unsigned short WORD;
typedef short SHORT;
__declspec(dllimport) HANDLE LoadLibraryA(const char *);
__declspec(dllimport) void *GetProcAddress(HANDLE, const char *);
__declspec(dllimport) HANDLE GetStdHandle(DWORD);
__declspec(dllimport) int WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
__declspec(dllimport) void ExitProcess(unsigned);
__declspec(dllimport) const char *GetCommandLineA(void);
typedef unsigned int UINT;
typedef struct { HANDLE hwnd; UINT message; unsigned long long wParam; long long lParam; DWORD time; long x, y; } MSG;
__declspec(dllimport) HANDLE CreateWindowExA(DWORD, const char *, const char *, DWORD, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
__declspec(dllimport) int PostMessageA(HANDLE, UINT, unsigned long long, long long);
__declspec(dllimport) int PeekMessageA(MSG *, HANDLE, UINT, UINT, UINT);
__declspec(dllimport) HANDLE SetFocus(HANDLE);
__declspec(dllimport) void Sleep(DWORD);
__declspec(dllimport) HANDLE ImmAssociateContext(HANDLE, HANDLE);
__declspec(dllimport) HANDLE ImmCreateContext(void);
__declspec(dllimport) void keybd_event(unsigned char, unsigned char, DWORD, unsigned long long);

// For --overlay: a window like the game's, redrawn every frame.
typedef long LONG; typedef long long LRESULT; typedef unsigned long long WPARAM; typedef long long LPARAM;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { UINT style; LRESULT (*proc)(HANDLE, UINT, WPARAM, LPARAM); int clsExtra, wndExtra; HANDLE instance, icon, cursor, background; const char *menu, *className; } WNDCLASSA;
typedef struct { HANDLE hdc; int erase; RECT paint; int restore, incUpdate; unsigned char reserved[32]; } PAINTSTRUCT;
__declspec(dllimport) unsigned short RegisterClassA(const WNDCLASSA *);
__declspec(dllimport) LRESULT DefWindowProcA(HANDLE, UINT, WPARAM, LPARAM);
__declspec(dllimport) LRESULT DispatchMessageA(const MSG *);
__declspec(dllimport) int ShowWindow(HANDLE, int);
__declspec(dllimport) int AdjustWindowRect(RECT *, DWORD, int);
__declspec(dllimport) int RedrawWindow(HANDLE, const RECT *, HANDLE, UINT);
__declspec(dllimport) int DestroyWindow(HANDLE);
__declspec(dllimport) HANDLE BeginPaint(HANDLE, PAINTSTRUCT *);
__declspec(dllimport) int EndPaint(HANDLE, const PAINTSTRUCT *);
__declspec(dllimport) HANDLE LoadImageW(HANDLE, const unsigned short *, UINT, int, int, UINT);
__declspec(dllimport) HANDLE GetDC(HANDLE);
__declspec(dllimport) int EnumWindows(int (*)(HANDLE, LPARAM), LPARAM);
__declspec(dllimport) DWORD GetWindowThreadProcessId(HANDLE, DWORD *);
__declspec(dllimport) int IsWindowVisible(HANDLE);
__declspec(dllimport) int GetWindowTextW(HANDLE, unsigned short *, int);
__declspec(dllimport) DWORD GetCurrentProcessId(void);
__declspec(dllimport) HANDLE CreateCompatibleDC(HANDLE);
__declspec(dllimport) HANDLE SelectObject(HANDLE, HANDLE);
__declspec(dllimport) int BitBlt(HANDLE, int, int, int, int, HANDLE, int, int, DWORD);
__declspec(dllimport) int GetDeviceCaps(HANDLE, int);
__declspec(dllimport) int QueryPerformanceCounter(long long *);
__declspec(dllimport) int QueryPerformanceFrequency(long long *);
__declspec(dllimport) HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
__declspec(dllimport) int ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
__declspec(dllimport) int CloseHandle(HANDLE);
__declspec(dllimport) int DeleteFileA(const char *);

typedef struct { DWORD packet; WORD buttons; unsigned char lt, rt; SHORT lx, ly, rx, ry; } STATE;
typedef struct { unsigned char type, subtype; WORD flags; WORD b; unsigned char lt, rt; SHORT lx, ly, rx, ry; WORD m1, m2; } CAPS;

static void out(const char *s) { DWORD n = 0, w; while (s[n]) n++; WriteFile(GetStdHandle((DWORD)-11), s, n, &w, 0); }
static void num(const char *label, long v) {
    char b[32]; int k = 0; unsigned long u = v < 0 ? -v : v;
    out(label);
    if (v < 0) out("-");
    do b[k++] = '0' + u % 10; while ((u /= 10) && k < 30);
    char r[32]; int j = 0; while (k) r[j++] = b[--k]; r[j] = 0; out(r); out("\r\n");
}

static HANDLE backdrop;
#define GAME_W 960
#define GAME_H 540
static LRESULT gameProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT: the backdrop
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        if (backdrop) { HANDLE m = CreateCompatibleDC(dc); HANDLE old = SelectObject(m, backdrop); BitBlt(dc, 0, 0, GAME_W, GAME_H, m, 0, 0, 0x00CC0020 /*SRCCOPY*/); SelectObject(m, old); }
        EndPaint(w, &ps);
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}
// The mod's overlay windows (by their titles) that are on screen. A window's title is
// asked for once (asking sends a message to the overlay thread and waits for it).
static int overlaysSeen, legendSeen, bannerSeen, known;
static HANDLE knownWnd[64]; static int knownKind[64];
static int countOverlay(HANDLE w, LPARAM unused) {
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId()) return 1;
    int kind = -1;
    for (int i = 0; i < known; i++) if (knownWnd[i] == w) kind = knownKind[i];
    if (kind < 0) {
        static const char *names[] = {"Keyboard controls", "Typing mode", "Typing mode frame"};
        unsigned short t[64]; int n = GetWindowTextW(w, t, 64);
        kind = 0;
        for (int k = 0; k < 3; k++) {
            const char *e = names[k]; int i = 0;
            while (i < n && e[i] && t[i] == (unsigned short)e[i]) i++;
            if (i == n && !e[i]) kind = k + 1;
        }
        if (known < 64) { knownWnd[known] = w; knownKind[known++] = kind; }
    }
    if (!kind || !IsWindowVisible(w)) return 1;
    overlaysSeen++;
    if (kind == 1) legendSeen = 1;
    if (kind == 2) bannerSeen = 1;
    return 1;
}
// A layout saved while the game runs: default.txt's text written to name (the mod
// takes the newest layout file within a second and shows "Key layout loaded").
static void saveLayout(const char *name) {
    static char text[65536]; DWORD n = 0, w;
    HANDLE f = CreateFileA("default.txt", 0x80000000 /*GENERIC_READ*/, 3, 0, 3 /*OPEN_EXISTING*/, 128, 0);
    if (f == (HANDLE)-1) { out("reload: no default.txt\r\n"); return; }
    ReadFile(f, text, sizeof(text), &n, 0); CloseHandle(f);
    f = CreateFileA(name, 0x40000000 /*GENERIC_WRITE*/, 3, 0, 2 /*CREATE_ALWAYS*/, 128, 0);
    if (f == (HANDLE)-1) { out("reload: can't write the layout\r\n"); return; }
    WriteFile(f, text, n, &w, 0); CloseHandle(f);
}
static int argNum(const char *cmd, char key, int fallback) {
    for (const char *p = cmd; *p; p++) {
        if (p[0] != ' ' || p[1] != key || p[2] != '=') continue;
        int v = 0; for (p += 3; *p >= '0' && *p <= '9'; p++) v = v * 10 + *p - '0';
        return v;
    }
    return fallback;
}
// --overlay: a 960x540 window like the game's in the bottom-right corner of the screen
// (bright.bmp, or dark.bmp with "dark", next to this exe), redrawn every frame, never
// brought to the front. At T ms it types T (typing mode), holds W from W to W+700 ms
// (the banner shakes), at E ms presses Esc ("Back to playing"), at O ms presses the
// toggle key (Backtick: "wasdmod off"), at N ms presses it again ("wasdmod on"), and
// ends at Z ms (defaults T=600 W=1600 E=3000 O=3300 N=4200 Z=4800; change them like
// "T=900"; a time past Z leaves that step out; X= and Y= move the window). At L ms it
// saves a layout (default.txt's text as wasdmod-test.txt: "Key layout loaded" drops in
// within a second), at R ms default.txt again (the banner shows "Official layout" and
// stays 4 s from then); both are left out by default, and wasdmod-test.txt is deleted at
// the end. It prints how long
// this window's frames took meanwhile: the overlays run on their own thread and
// shouldn't make them late. With RequireFocus=0 in default.txt the overlays show even
// though this window isn't in front (KeepCursorInWindow=0 and MouseMoveSwitches=0 too:
// the bottle shares the real mouse).
// --overlay focus: the same with RequireFocus=1: as this window isn't in front, none of
// the overlays may show (they'd float over other apps). (The toggle key isn't taken
// either then, so the mod doesn't switch off.)
static void overlayTest(int dark, int focusCase, const char *cmd) {
    int tT = argNum(cmd, 'T', 600), tW = argNum(cmd, 'W', 1600), tE = argNum(cmd, 'E', 3000), tEnd = argNum(cmd, 'Z', 4800);
    int tO = argNum(cmd, 'O', 3300), tN = argNum(cmd, 'N', 4200), tL = argNum(cmd, 'L', 99999), tR = argNum(cmd, 'R', 99999);
    num("timeline: T ", tT); num("  W ", tW); num("  E ", tE); num("  O ", tO); num("  N ", tN); num("  L ", tL); num("  R ", tR); num("  Z ", tEnd);
    static const unsigned short bright[] = {'b','r','i','g','h','t','.','b','m','p',0}, darkName[] = {'d','a','r','k','.','b','m','p',0};
    backdrop = LoadImageW(0, dark ? darkName : bright, 0 /*IMAGE_BITMAP*/, 0, 0, 0x10 /*LR_LOADFROMFILE*/);
    WNDCLASSA wc; char *z = (char *)&wc; for (unsigned i = 0; i < sizeof(wc); i++) z[i] = 0;
    wc.proc = gameProc; wc.className = "WasdTestGame";
    RegisterClassA(&wc);
    RECT r = {0, 0, GAME_W, GAME_H}; AdjustWindowRect(&r, 0x00CF0000 /*WS_OVERLAPPEDWINDOW*/, 0);
    // The screen in window coordinates (GetSystemMetrics: the screen DC's size can differ,
    // e.g. under CrossOver's Retina mode); X= and Y= place the window elsewhere.
    HANDLE screen = GetDC(0), u32 = LoadLibraryA("user32.dll");
    int (*metrics)(int) = u32 ? GetProcAddress(u32, "GetSystemMetrics") : 0;
    int sw = metrics ? metrics(0 /*SM_CXSCREEN*/) : screen ? GetDeviceCaps(screen, 8 /*HORZRES*/) : 1440;
    int sh = metrics ? metrics(1 /*SM_CYSCREEN*/) : screen ? GetDeviceCaps(screen, 10 /*VERTRES*/) : 900;
    int ww = r.right - r.left, wh = r.bottom - r.top;
    num("screen ", sw); num("  x ", sh);
    HANDLE w = CreateWindowExA(0x08000000 /*NOACTIVATE*/, "WasdTestGame", "wasd test game", 0x00CF0000, argNum(cmd, 'X', sw - ww - 16), argNum(cmd, 'Y', sh - wh - 80), ww, wh, 0, 0, 0, 0);
    ShowWindow(w, 4 /*SW_SHOWNOACTIVATE*/);
    HANDLE m = LoadLibraryA(".\\xinput1_4.dll");
    DWORD (*get)(DWORD, STATE *) = m ? GetProcAddress(m, "XInputGetState") : 0;
    if (!get) { out("FAIL load\r\n"); DestroyWindow(w); ExitProcess(1); }
    #define PARTS 7
    struct { int from, to; const char *name; } parts[PARTS] = {
        {300, tT, "before typing"}, {tT, tT + 1000, "typing: banner drops in, frame traces"}, {tT + 1000, tW, "typing, idle (caret blinks)"},
        {tW + 400, tW + 1100, "typing: banner shakes, frame blinks"}, {tE, tE + 300, "Esc: fade out, \"Back to playing\""},
        {tO, tO + 600, "toggle key: \"wasdmod off\" drops in"}, {tN, tN + 600, "toggle key again: \"wasdmod on\""}};
    static long long sum[PARTS], worst[PARTS]; static int frames[PARTS], late[PARTS], worstAt[PARTS], lateAt[24], lateUs[24], lates, maxSeen;
    long long f, start, last, now; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&start); last = start;
    int typed = 0, held = 0, released = 0, escaped = 0, toggles = 0, saved = 0, resaved = 0;
    for (;;) {
        MSG msg; while (PeekMessageA(&msg, 0, 0, 0, 1)) DispatchMessageA(&msg);
        STATE s; get(0, &s);
        RedrawWindow(w, 0, 0, 0x1 | 0x100 /*RDW_INVALIDATE|RDW_UPDATENOW*/);
        overlaysSeen = 0; EnumWindows(countOverlay, 0);
        if (overlaysSeen > maxSeen) maxSeen = overlaysSeen;
        long long frameEnd = last + f / 60;
        do { Sleep(1); QueryPerformanceCounter(&now); } while (now < frameEnd);
        int ms = (int)((now - start) * 1000 / f), dt = (int)((now - last) * 1000000 / f); // microseconds
        last = now;
        if (dt > 25000 && ms > 1000 && lates < 24) { lateAt[lates] = ms; lateUs[lates++] = dt; }
        for (int p = 0; p < PARTS; p++) if (ms >= parts[p].from && ms < parts[p].to) { sum[p] += dt; frames[p]++; if (dt > worst[p]) { worst[p] = dt; worstAt[p] = ms; } if (dt > 25000) late[p]++; }
        if (!typed && ms >= tT) { typed = 1; PostMessageA(w, 0x100, 'T', 0x00140001); }
        if (!held && ms >= tW) { held = 1; keybd_event('W', 0x11, 0, 0); }
        if (!released && ms >= tW + 700) { released = 1; keybd_event('W', 0x11, 2 /*KEYUP*/, 0); }
        if (!escaped && ms >= tE) { escaped = 1; PostMessageA(w, 0x100, 0x1B, 0x00010001); }
        if (!saved && ms >= tL) { saved = 1; saveLayout("wasdmod-test.txt"); }
        if (!resaved && ms >= tR) { resaved = 1; saveLayout("default.txt"); }
        // The toggle key (Backtick), held for 80 ms: down at O, up, down at N, up.
        int at[4] = {tO, tO + 80, tN, tN + 80};
        if (toggles < 4 && ms >= at[toggles] && at[toggles] < tEnd) { keybd_event(0xC0, 0x29, toggles & 1 ? 2 /*KEYUP*/ : 0, 0); toggles++; }
        if (ms >= tEnd) break;
    }
    if (held && !released) keybd_event('W', 0x11, 2, 0);
    if (toggles & 1) keybd_event(0xC0, 0x29, 2, 0); // never left down
    DestroyWindow(w);
    if (saved) DeleteFileA("wasdmod-test.txt");
    if (focusCase) {
        num("focus: overlay windows seen while the game window wasn't in front (0 expected) ", maxSeen);
        out(maxSeen ? "RESULT FAIL\r\n" : "RESULT PASS\r\n");
        ExitProcess(maxSeen != 0);
    }
    num("key list shown ", legendSeen); num("typing banner shown ", bannerSeen);
    for (int i = 0; i < lates; i++) { num("late frame at ms ", lateAt[i]); num("  took us ", lateUs[i]); }
    for (int p = 0; p < PARTS; p++) {
        out(parts[p].name); num(": frames ", frames[p]);
        num("  average us ", frames[p] ? (long)(sum[p] / frames[p]) : 0); num("  worst us ", (long)worst[p]); num("  worst at ms ", worstAt[p]);
        num("  over 25 ms ", late[p]);
    }
    ExitProcess(0);
}

void mainCRTStartup(void) {
    int fail = 0;
    {
        const char *c = GetCommandLineA(); int overlay = 0, dark = 0, focus = 0;
        for (const char *p = c; *p; p++) {
            if (p[0] == '-' && p[1] == '-' && p[2] == 'o' && p[3] == 'v' && p[4] == 'e' && p[5] == 'r') overlay = 1;
            if (p[0] == ' ' && p[1] == 'd' && p[2] == 'a' && p[3] == 'r' && p[4] == 'k') dark = 1;
            if (p[0] == ' ' && p[1] == 'f' && p[2] == 'o' && p[3] == 'c' && p[4] == 'u' && p[5] == 's') focus = 1;
        }
        if (overlay) overlayTest(dark, focus, c);
    }
    HANDLE m = LoadLibraryA(".\\xinput1_4.dll");
    if (!m) { out("FAIL load\r\n"); ExitProcess(1); }
    DWORD (*get)(DWORD, STATE *) = GetProcAddress(m, "XInputGetState");
    DWORD (*getEx)(DWORD, STATE *) = GetProcAddress(m, (const char *)100);
    DWORD (*caps)(DWORD, DWORD, CAPS *) = GetProcAddress(m, "XInputGetCapabilities");
    DWORD (*set)(DWORD, void *) = GetProcAddress(m, "XInputSetState");
    if (!get || !getEx || !caps || !set) { out("FAIL exports\r\n"); ExitProcess(1); }
    STATE s; CAPS c; WORD vib[2] = {0, 0};
    DWORD r = get(0, &s); num("GetState(0) result ", r); num("  stick X ", s.lx); num("  stick Y ", s.ly);
    if (r != 0 || s.lx || s.ly) fail = 1;
    r = getEx(0, &s); num("GetStateEx(0) result ", r); if (r != 0) fail = 1;
    r = caps(0, 0, &c); num("GetCapabilities(0) result ", r); num("  type ", c.type); if (r != 0 || c.type != 1) fail = 1;
    r = set(0, vib); num("SetState(0) result ", r); if (r != 0) fail = 1;
    r = get(1, &s); num("GetState(1) result (1167 = no controller) ", r);
    HANDLE builtin = LoadLibraryA("xinput1_3.dll"); num("builtin xinput1_3 present ", builtin != 0); if (!builtin) fail = 1;
    // Key hiding: messages posted to this test's own window only.
    HANDLE wnd = CreateWindowExA(0, "STATIC", "wasd test", 0, 0, 0, 10, 10, 0, 0, 0, 0);
    if (!wnd) { out("FAIL window\r\n"); fail = 1; }
    else {
        // Like Unreal with no text box focused: no IMM context on the window.
        SetFocus(wnd);
        HANDLE imc = ImmAssociateContext(wnd, 0);
        get(0, &s); Sleep(60); // text box state refreshes every 50 ms
        for (int i = 0; i < 240; i++) get(0, &s); // the mod rescans windows every 240 polls
        MSG msg; UINT m[4]; unsigned long long k[4];
        PostMessageA(wnd, 0x100, 'W', 0x00110001); // W keydown, scan code 0x11
        PostMessageA(wnd, 0x100, 'P', 0x00190001);
        PostMessageA(wnd, 0x200, 0, 0x00100010); // WM_MOUSEMOVE
        for (int i = 0; i < 4; i++) { m[i] = 0xffff; k[i] = 0; if (PeekMessageA(&msg, wnd, 0, 0, 1)) { m[i] = msg.message; k[i] = msg.wParam; } }
        // Expected: W hidden (0), E keydown (256), then W's typed character (258 'w').
        num("gameplay: W keydown arrives as message ", m[0]);
        num("gameplay: P (unused) keydown arrives as message (0 = blocked) ", m[1]);
        num("gameplay: mouse move arrives as message (0 = hidden) ", m[2]);
        num("gameplay: typed character for W (65535 = none; TypedCharacters=0) ", m[3]);
        if (m[0] != 0 || m[1] != 0 || m[2] != 0 || m[3] != 0xffff) fail = 1;
        // Typing mode: T starts it (swallowed); W, T and Enter then reach the game; Esc ends it (and passes).
        {
            UINT t[6];
            PostMessageA(wnd, 0x100, 'T', 0x00140001);
            PostMessageA(wnd, 0x100, 'W', 0x00110001);
            PostMessageA(wnd, 0x100, 'T', 0x00140001);
            PostMessageA(wnd, 0x100, 0x0D, 0x001C0001);
            PostMessageA(wnd, 0x100, 0x1B, 0x00010001);
            PostMessageA(wnd, 0x100, 'W', 0x00110001);
            for (int i = 0; i < 6; i++) { t[i] = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) t[i] = msg.message; }
            num("typing: first T arrives as message (0 = swallowed, typing on) ", t[0]);
            num("typing: W while typing arrives as message (256 = passes) ", t[1]);
            num("typing: T while typing arrives as message (256 = just a letter) ", t[2]);
            num("typing: Enter arrives as message (256 = passes, still typing) ", t[3]);
            num("typing: Esc arrives as message (0 = kept from the game, typing off) ", t[4]);
            num("typing: W after typing arrives as message (0 = hidden again) ", t[5]);
            if (t[0] != 0 || t[1] != 0x100 || t[2] != 0x100 || t[3] != 0x100 || t[4] != 0 || t[5] != 0) fail = 1;
        }
        // Menu shortcut: passes through and switches to mouse mode; clicks stay
        // in mouse mode; a movement key switches back and is hidden again.
        PostMessageA(wnd, 0x100, 'M', 0x00320001);
        PostMessageA(wnd, 0x201, 1, 0x00100010); // left click
        PostMessageA(wnd, 0x100, 'W', 0x00110001);
        PostMessageA(wnd, 0x100, 'W', 0x00110001);
        for (int i = 0; i < 4; i++) { m[i] = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) m[i] = msg.message; }
        num("menu: M keydown arrives as message (256 = passes) ", m[0]);
        num("menu: left click arrives as message (513 = passes) ", m[1]);
        num("menu: first W arrives as message (0 = hidden, back to controller) ", m[2]);
        num("menu: second W arrives as message (0 = hidden) ", m[3]);
        if (m[0] != 0x100 || m[1] != 0x201 || m[2] != 0 || m[3] != 0) fail = 1;
        // Back key: M opens mouse mode, Esc (passed to the game) returns to controller mode.
        PostMessageA(wnd, 0x100, 'M', 0x00320001);
        PostMessageA(wnd, 0x100, 0x1B, 0x00010001);
        PostMessageA(wnd, 0x200, 0, 0x00100010);
        { UINT e[3]; for (int i = 0; i < 3; i++) { e[i] = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) e[i] = msg.message; }
          num("back: Esc in mouse mode arrives as message (256 = the game gets it) ", e[1]);
          num("back: mouse move after Esc arrives as message (0 = controller mode again) ", e[2]);
          if (e[0] != 0x100 || e[1] != 0x100 || e[2] != 0) fail = 1; }
        // Disabled keys (DisabledKeys=..., none by default): print only. With U listed,
        // U in mouse mode arrives as 0 (off); otherwise 256.
        PostMessageA(wnd, 0x100, 'M', 0x00320001);                 // mouse mode
        PostMessageA(wnd, 0x100, 'U', 0x00160001);
        { UINT e[2]; for (int i = 0; i < 2; i++) { e[i] = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) e[i] = msg.message; }
          num("disabled: U in mouse mode arrives as message (0 = off if listed) ", e[1]); }
        // Remap (Tab=S by default, the menu wheel): Tab itself is hidden from the game.
        // (The S it sends only goes out while the game window is in front, so this test
        // can't see it.)
        PostMessageA(wnd, 0x100, 0x09, 0x000F0001);
        PostMessageA(wnd, 0x101, 0x09, 0xC00F0001); // and up again, so the S it sent is released
        { UINT b = 0xffff, u = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) b = msg.message; if (PeekMessageA(&msg, wnd, 0, 0, 1)) u = msg.message;
          num("remap: Tab arrives as message (0 = hidden, S sent instead) ", b); if (b != 0 || u != 0) fail = 1; }
        // Bow on right click (the default): holding it is keyboard mode (aim with the mouse); after
        // release WASD returns to controller mode. (WASD being ignored while it's held
        // needs a physically held button, which a posted message can't fake.)
        PostMessageA(wnd, 0x100, 'W', 0x00110001);                 // back to controller mode
        PostMessageA(wnd, 0x204, 0x02, 0x00100010);                // right button down
        PostMessageA(wnd, 0x200, 0, 0x00100010);
        PostMessageA(wnd, 0x205, 0, 0x00100010);                   // right button up
        PostMessageA(wnd, 0x100, 'W', 0x00110001);
        PostMessageA(wnd, 0x200, 0, 0x00100010);
        { UINT e[6]; for (int i = 0; i < 6; i++) { e[i] = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) e[i] = msg.message; }
          num("bow: right button down arrives as message (516 = the game gets it) ", e[1]);
          num("bow: mouse move while aiming arrives as message (512 = passes) ", e[2]);
          num("bow: right button up arrives as message (517 = passes) ", e[3]);
          num("bow: W after release arrives as message (0 = hidden, controller mode) ", e[4]);
          num("bow: mouse move after that arrives as message (0 = hidden) ", e[5]);
          if (e[0] != 0 || e[1] != 0x204 || e[2] != 0x200 || e[3] != 0x205 || e[4] != 0 || e[5] != 0) fail = 1; }
        // Like Unreal with a text box focused: the window gets an IMM context back.
        ImmAssociateContext(wnd, imc ? imc : ImmCreateContext());
        Sleep(60); get(0, &s);
        num("text box: stick Y while focused (0 expected) ", s.ly);
        PostMessageA(wnd, 0x100, 'W', 0x00110001);
        UINT t = 0xffff; if (PeekMessageA(&msg, wnd, 0, 0, 1)) t = msg.message;
        num("text box: W keydown arrives as message (256 = passes through) ", t);
        if (t != 0x100 || s.ly) fail = 1;
        ImmAssociateContext(wnd, 0);
        Sleep(120); get(0, &s); // text box checks are reused for 50 ms
    }
    // Opt-in: injects real key events, so only run with the game closed (in a test
    // bottle: a bottle shares its key state). Needs RequireFocus=0 in default.txt,
    // since this console test has no window, and MouseMoveSwitches=0, since the
    // real mouse moving meanwhile switches to mouse mode. Each step waits for the
    // movement smoothing (AccelMs, TurnMs, DecelMs) to settle.
    const char *cmd = GetCommandLineA(); int keys = 0;
    for (const char *p = cmd; *p; p++) if (p[0] == '-' && p[1] == '-' && p[2] == 'k' && p[3] == 'e' && p[4] == 'y' && p[5] == 's') keys = 1;
    if (!keys) { out(fail ? "RESULT FAIL\r\n" : "RESULT PASS (basic; run with --keys for the key test)\r\n"); ExitProcess(fail); }
    DWORD before = s.packet;
    keybd_event(0x57, 0, 0, 0);                  // W down
    get(0, &s); Sleep(200); get(0, &s); num("W held: stick X ", s.lx); num("W held: stick Y ", s.ly);
    if (s.lx != 0 || s.ly != 32767) fail = 1;
    keybd_event(0x44, 0, 0, 0);                  // D down as well
    get(0, &s); Sleep(200); get(0, &s); num("W+D held: stick X ", s.lx); num("W+D held: stick Y ", s.ly);
    if (s.lx < 23168 || s.lx > 23172 || s.ly < 23168 || s.ly > 23172) fail = 1; // 0.7071 of full, give or take rounding
    keybd_event(0x57, 0, 2, 0); keybd_event(0x44, 0, 2, 0); // release both
    get(0, &s); Sleep(200); get(0, &s); num("released: stick X ", s.lx); num("released: stick Y ", s.ly);
    if (s.lx || s.ly) fail = 1;
    num("packet number advanced ", s.packet != before); if (s.packet == before) fail = 1;
    out(fail ? "RESULT FAIL\r\n" : "RESULT PASS\r\n");
    ExitProcess(fail);
}
