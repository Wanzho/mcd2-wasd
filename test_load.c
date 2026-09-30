// Loads the mod DLL from its own folder and checks the exports behave:
// controller 0 appears connected, the stick is centred with no keys held,
// and the builtin XInput backend is reachable.
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

void mainCRTStartup(void) {
    int fail = 0;
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
        get(0, &s);
    }
    // Opt-in: injects real key events, so only run with the game closed.
    // Needs RequireFocus=0 in default.txt, since this console test has no window.
    const char *cmd = GetCommandLineA(); int keys = 0;
    for (const char *p = cmd; *p; p++) if (p[0] == '-' && p[1] == '-' && p[2] == 'k' && p[3] == 'e' && p[4] == 'y' && p[5] == 's') keys = 1;
    if (!keys) { out(fail ? "RESULT FAIL\r\n" : "RESULT PASS (basic; run with --keys for the key test)\r\n"); ExitProcess(fail); }
    DWORD before = s.packet;
    keybd_event(0x57, 0, 0, 0);                  // W down
    get(0, &s); num("W held: stick X ", s.lx); num("W held: stick Y ", s.ly);
    if (s.lx != 0 || s.ly != 32767) fail = 1;
    keybd_event(0x44, 0, 0, 0);                  // D down as well
    get(0, &s); num("W+D held: stick X ", s.lx); num("W+D held: stick Y ", s.ly);
    if (s.lx != 23170 || s.ly != 23170) fail = 1;
    keybd_event(0x57, 0, 2, 0); keybd_event(0x44, 0, 2, 0); // release both
    get(0, &s); num("released: stick X ", s.lx); num("released: stick Y ", s.ly);
    if (s.lx || s.ly) fail = 1;
    num("packet number advanced ", s.packet != before); if (s.packet == before) fail = 1;
    out(fail ? "RESULT FAIL\r\n" : "RESULT PASS\r\n");
    ExitProcess(fail);
}
