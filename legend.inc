// On-screen key legend: a click-through, semi-transparent panel pinned to the
// top-left of the game window. It is shown for LegendSeconds after startup,
// briefly after the toggle key, and whenever the legend key turns it on. It
// runs on its own thread with its own message loop, so it never blocks the game.

typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { UINT style; LRESULT (*proc)(HANDLE, UINT, WPARAM, LPARAM); int clsExtra, wndExtra; HANDLE instance, icon, cursor, background; const char *menu, *className; } WNDCLASSA;
typedef struct { HANDLE hdc; BOOL erase; RECT paint; BOOL restore, incUpdate; BYTE reserved[32]; } PAINTSTRUCT;

IMP WORD RegisterClassA(const WNDCLASSA *);
IMP HANDLE CreateWindowExA(DWORD, const char *, const char *, DWORD, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
IMP LRESULT DefWindowProcA(HANDLE, UINT, WPARAM, LPARAM);
IMP BOOL ShowWindow(HANDLE, int);
IMP BOOL SetWindowPos(HANDLE, HANDLE, int, int, int, int, UINT);
IMP BOOL GetClientRect(HANDLE, RECT *);
IMP BOOL ClientToScreen(HANDLE, POINT *);
IMP BOOL SetLayeredWindowAttributes(HANDLE, DWORD, BYTE, DWORD);
IMP BOOL GetMessageA(MSG *, HANDLE, UINT, UINT);
IMP LRESULT DispatchMessageA(const MSG *);
IMP U64 SetTimer(HANDLE, U64, UINT, void *);
IMP BOOL InvalidateRect(HANDLE, const RECT *, BOOL);
IMP HANDLE BeginPaint(HANDLE, PAINTSTRUCT *);
IMP BOOL EndPaint(HANDLE, const PAINTSTRUCT *);
IMP int FillRect(HANDLE, const RECT *, HANDLE);
IMP BOOL IsWindowVisible(HANDLE);
IMP HANDLE GetDC(HANDLE);
IMP int ReleaseDC(HANDLE, HANDLE);
IMP HANDLE CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *);
IMP HANDLE SelectObject(HANDLE, HANDLE);
IMP DWORD SetTextColor(HANDLE, DWORD);
IMP int SetBkMode(HANDLE, int);
IMP BOOL TextOutW(HANDLE, int, int, const WCHAR *, int);
IMP HANDLE CreateSolidBrush(DWORD);
IMP BOOL GetTextExtentPoint32W(HANDLE, const WCHAR *, int, SIZE *);
IMP int GetTextFaceA(HANDLE, int, char *);
IMP BOOL SetCursorPos(int, int);
IMP BOOL ClipCursor(const RECT *);
IMP BOOL DeleteObject(HANDLE);

#define RGB(r, g, b) ((DWORD)(r) | ((DWORD)(g) << 8) | ((DWORD)(b) << 16))
#define LEGEND_LINES 32
#define LINE_BYTES 200 // UTF-8, with room for the longer languages
static char legendText[LEGEND_LINES][LINE_BYTES];
static int legendCount, legendW, legendH, lineH = 18;
static HANDLE legendWnd, legendFont, legendBrush;
static int legendShown;

static void addLine(const char *key, const char *what) {
    if (legendCount >= LEGEND_LINES) return;
    char *l = legendText[legendCount++]; l[0] = 0;
    append(l, LINE_BYTES, key); append(l, LINE_BYTES, "  ");
    append(l, LINE_BYTES, what);
}
// The text is UTF-8; Windows draws it as UTF-16.
static int widen(const char *s, WCHAR *out, int cap) { int n = MultiByteToWideChar(65001 /*CP_UTF8*/, 0, s, -1, out, cap); return n > 0 ? n - 1 : 0; }
static void textOut(HANDLE dc, int x, int y, const char *s) { WCHAR w[LINE_BYTES]; TextOutW(dc, x, y, w, widen(s, w, LINE_BYTES)); }
static BOOL textSize(HANDLE dc, const char *s, SIZE *sz) { WCHAR w[LINE_BYTES]; return GetTextExtentPoint32W(dc, w, widen(s, w, LINE_BYTES), sz); }
// A translated "{action} (sends the game's {key})" with its parts filled in.
static void fill(char *out, unsigned cap, const char *text, const char *action, const char *key) {
    out[0] = 0;
    for (const char *p = text; *p; ) {
        if (startsWith(p, "{action}")) { append(out, cap, action); p += 8; continue; }
        if (startsWith(p, "{key}")) { append(out, cap, key); p += 5; continue; }
        char c[2] = {*p++, 0}; append(out, cap, c);
    }
}
// The system's own font for Chinese, Japanese and Korean (the names on Windows, then
// on a Mac and on Linux), Arial for the rest.
static HANDLE makeFont(int height, int weight) {
    static const struct { const char *code; DWORD charset; const char *faces[6]; } cjk[] = {
        {"zh-Hans", 134 /*GB2312*/, {"Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Hiragino Sans GB", "Noto Sans CJK SC", "Source Han Sans SC"}},
        {"zh-Hant", 136 /*BIG5*/, {"Microsoft JhengHei UI", "Microsoft JhengHei", "PingFang TC", "Heiti TC", "Noto Sans CJK TC", "Source Han Sans TC"}},
        {"ja", 128 /*SHIFTJIS*/, {"Meiryo UI", "Yu Gothic UI", "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Noto Sans CJK JP", "Source Han Sans"}},
        {"ko", 129 /*HANGUL*/, {"Malgun Gothic", "Apple SD Gothic Neo", "AppleGothic", "Noto Sans CJK KR", "Source Han Sans K", 0}},
    };
    for (unsigned i = 0; i < sizeof(cjk) / sizeof(*cjk); i++) {
        if (!sameStr(cjk[i].code, LANG_CODE[langIndex])) continue;
        HANDLE dc = GetDC(0);
        for (int f = 0; f < 6 && cjk[i].faces[f]; f++) {
            HANDLE font = CreateFontA(height, 0, 0, 0, weight, 0, 0, 0, cjk[i].charset, 0, 0, 5 /*CLEARTYPE*/, 0, cjk[i].faces[f]);
            char got[64] = {0}; HANDLE old = SelectObject(dc, font); GetTextFaceA(dc, sizeof(got), got); SelectObject(dc, old);
            int k = 0; while (got[k] && lower(got[k]) == lower(cjk[i].faces[f][k])) k++;
            if (!got[k] && !cjk[i].faces[f][k]) { ReleaseDC(0, dc); return font; }
            DeleteObject(font);
        }
        ReleaseDC(0, dc);
        return CreateFontA(height, 0, 0, 0, weight, 0, 0, 0, cjk[i].charset, 0, 0, 5, 0, ""); // any font with these letters
    }
    return CreateFontA(height, 0, 0, 0, weight, 0, 0, 0, 1 /*DEFAULT_CHARSET*/, 0, 0, 5, 0, "Arial");
}

// The game action a converted key presses, from the layout's [GameKeys] (the
// game's own keyboard keys, written by the key layout editor): S -> "Menu wheel"
// (in English; the key list translates it).
static const char *gameActionFor(BYTE vk) {
    static const char *names[][2] = {
        {"wheel", N_("Menu wheel")}, {"inventory", N_("Inventory")}, {"map", N_("World map")}, {"quest", N_("Quests")},
        {"social", N_("Social menu")}, {"emotes", N_("Emotes")}, {"collect", N_("Collectibles")}, {"events", N_("Event log")},
        {"tpplayer", N_("Teleport to player")}, {"statue", N_("Teleport statue")}, {"tp1", N_("Teleport to player 1")},
        {"tp2", N_("Teleport to player 2")}, {"tp3", N_("Teleport to player 3")}, {"tp4", N_("Teleport to player 4")},
        {"ranged", N_("Bow")}, {"potion", N_("Health potion")}, {"jump", N_("Jump")}, {"melee", N_("Melee")},
        {"dodge", N_("Directional dodge")}, {"fdodge", N_("Forward dodge")}, {"art1", N_("Artifact 1")}, {"art2", N_("Artifact 2")},
        {"art3", N_("Artifact 3")}, {"trail", N_("Guidance trail")}, {"heavy", N_("Heavy jump attack")}, {"root", N_("Root")},
    };
    static char sec[2048];
    if (!GetPrivateProfileSectionA("GameKeys", sec, sizeof(sec), ini)) return 0;
    for (const char *p = sec; *p; p += len(p) + 1) { // "wheel=S", "quest=V, J"
        const char *eq = p; while (*eq && *eq != '=') eq++;
        if (!*eq) continue;
        char keys[64] = {0}; append(keys, sizeof(keys), eq + 1);
        for (char *k = keys; *k; ) {
            while (*k == ' ' || *k == ',') k++;
            char *e = k; while (*e && *e != ',') e++;
            char *end = e; while (end > k && end[-1] == ' ') end--;
            char save = *end; *end = 0;
            int hit = keyCode(k) == vk;
            *end = save; k = e;
            if (!hit) continue;
            for (unsigned i = 0; i < sizeof(names) / sizeof(*names); i++) {
                const char *a = p, *n = names[i][0];
                while (a < eq && *n && lower(*a) == *n) a++, n++;
                if (a == eq && !*n) return names[i][1];
            }
        }
    }
    return 0;
}

static void buildLegend(void) {
    legendCount = 0;
    char move[64] = {0};
    append(move, sizeof(move), moveUp.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveLeft.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveDown.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveRight.text);
    addLine(move, T("Move"));
    if (dodgeMouse.text[0]) addLine(dodgeMouse.text, T("Click: dodge. Click + drag: roll that way"));
    for (int i = 0; i < NBTN; i++) {
        if (!btn[i].text[0]) continue;
        char what[LINE_BYTES] = {0}; append(what, sizeof(what), T(btnLabel[i]));
        append(what, sizeof(what), "  ["); append(what, sizeof(what), btnName[i]); append(what, sizeof(what), "]");
        addLine(btn[i].text, what);
    }
    // Converted keys, named after what they do: "Tab  Menu wheel (sends the game's S)".
    for (int i = 0; i < remapCount; i++) {
        char what[LINE_BYTES]; const char *name = gameActionFor(remapList[i].toVk);
        if (name) fill(what, sizeof(what), T("{action} (sends the game's {key})"), T(name), remapList[i].to);
        else fill(what, sizeof(what), T("Sends the game's {key}"), "", remapList[i].to);
        addLine(remapList[i].from, what);
    }
    if (typeText[0]) addLine(typeText, T("Type (Esc to stop)"));
    if (cursorKey.text[0]) addLine(cursorKey.text, cfg.cursorToggle ? T("Press: mouse cursor on / off") : T("Hold: mouse cursor"));
    if (menuText[0]) addLine(menuText, T("Game menus (mouse mode; move or fight to leave)"));
    if (toggleKey.text[0]) addLine(toggleKey.text, T("Keyboard & mouse on/off"));
    if (legendKey.text[0]) addLine(legendKey.text, T("Show / hide this list"));
}

// The game's main window: the largest visible window of this process that
// isn't the legend itself.
static HANDLE gameWnd; static LONG gameArea;
static BOOL findGameWindow(HANDLE w, LPARAM unused) {
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || w == legendWnd || !IsWindowVisible(w)) return 1;
    RECT r; GetClientRect(w, &r);
    LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area > gameArea) { gameArea = area; gameWnd = w; }
    return 1;
}

static void placeLegend(void) {
    U64 now = GetTickCount64();
    int want = cfg.legend && (legendVisible || now < legendUntil);
    gameWnd = 0; gameArea = 0;
    EnumWindows(findGameWindow, 0);
    if (gameWnd && gameArea >= 200 * 200) {
        RECT r; GetClientRect(gameWnd, &r); POINT o = {0, 0}; ClientToScreen(gameWnd, &o);
        gameLeft = o.x; gameTop = o.y; gameWidth = r.right - r.left; gameHeight = r.bottom - r.top;
    } else { gameWnd = 0; gameWidth = 0; }
    if (want && !gameWnd) want = 0;
    // If something (e.g. Cmd+backtick) brings the key list to the front, hide it so
    // the game gets focus and the mouse back.
    if (legendShown && GetForegroundWindow() == legendWnd) { want = 0; legendVisible = 0; legendUntil = 0; logline("Key list was brought to the front: hidden"); }
    if (want && cfg.requireFocus) { DWORD pid = 0; HANDLE f = GetForegroundWindow(); if (f) GetWindowThreadProcessId(f, &pid); if (pid != GetCurrentProcessId()) want = 0; }
    if (!want) { if (legendShown) { ShowWindow(legendWnd, 0 /*SW_HIDE*/); legendShown = 0; } return; }
    POINT p = {16, 16}; ClientToScreen(gameWnd, &p);
    SetWindowPos(legendWnd, (HANDLE)-1 /*HWND_TOPMOST*/, p.x, p.y, legendW, legendH, 0x10 | 0x40 /*SWP_NOACTIVATE|SWP_SHOWWINDOW*/);
    if (!legendShown) { InvalidateRect(legendWnd, 0, 1); legendShown = 1; }
}

static LRESULT legendProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        RECT r = {0, 0, legendW, legendH}; FillRect(dc, &r, legendBrush);
        SelectObject(dc, legendFont); SetBkMode(dc, 1 /*TRANSPARENT*/);
        SetTextColor(dc, RGB(255, 214, 102));
        textOut(dc, 10, 8, T("Keyboard controls"));
        SetTextColor(dc, RGB(240, 240, 240));
        for (int i = 0; i < legendCount; i++) textOut(dc, 10, 8 + (i + 1) * lineH + 4, legendText[i]);
        EndPaint(w, &ps);
        return 0;
    }
    if (msg == 0x113) { placeLegend(); return 0; } // WM_TIMER
    if (msg == 0x84) return -1;                     // WM_NCHITTEST: HTTRANSPARENT, clicks go to the game
    return DefWindowProcA(w, msg, wp, lp);
}

// A hidden 1x1 window whose 30 ms timer keeps the cursor in the game window, shows
// the typing banner and does the cursor-key nudge.
static HANDLE tickWnd;

// Typing-mode banner: a small bar at the top of the game window while typing mode
// is on. Placed once when typing starts and hidden when it ends (a window moving
// over the game every tick makes CrossOver stutter).
static HANDLE typeWnd, typeFont, typeBrush, typeEdge;
static int typeW, typeH, typeShown;
static LRESULT typeProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        RECT r = {0, 0, typeW, typeH}; FillRect(dc, &r, typeEdge);
        RECT in = {3, 3, typeW - 3, typeH - 3}; FillRect(dc, &in, typeBrush);
        SelectObject(dc, typeFont); SetBkMode(dc, 1 /*TRANSPARENT*/); SetTextColor(dc, RGB(255, 214, 102));
        const char *text = T("TYPING  -  Esc to finish"); SIZE sz;
        if (textSize(dc, text, &sz)) textOut(dc, (typeW - sz.cx) / 2, (typeH - sz.cy) / 2, text);
        EndPaint(w, &ps);
        return 0;
    }
    if (msg == 0x84) return -1; // WM_NCHITTEST: clicks go to the game
    return DefWindowProcA(w, msg, wp, lp);
}
static void showTyping(int want) {
    if (!typeWnd || want == typeShown) return;
    if (!want) { ShowWindow(typeWnd, 0 /*SW_HIDE*/); typeShown = 0; return; }
    if (!gameWnd || gameWidth < 400) return;
    POINT p = {0, 0}; ClientToScreen(gameWnd, &p);
    SetWindowPos(typeWnd, (HANDLE)-1 /*HWND_TOPMOST*/, p.x + (gameWidth - typeW) / 2, p.y + 24, typeW, typeH, 0x10 | 0x40 /*SWP_NOACTIVATE|SWP_SHOWWINDOW*/);
    InvalidateRect(typeWnd, 0, 1);
    typeShown = 1;
}

static LRESULT tickProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x113) { // WM_TIMER, every 30 ms
        showTyping(typingManual && !paused);
        int inControl = gameWnd && active();
        // Keep the mouse inside the game while playing. The game hides its cursor in
        // controller mode, so while you move with the keys the invisible cursor is
        // quietly moved back to the middle once it has drifted far (a third of the
        // window): rare, cheap, and it can't escape to other apps. Never while a mouse button is held (drag dodges
        // measure mouse travel), and only while the game window is in front.
        DWORD fgPid = 0; HANDLE fg = GetForegroundWindow(); if (fg) GetWindowThreadProcessId(fg, &fgPid);
        int sane = gameWnd && gameWidth >= 400 && gameHeight >= 300 && fgPid == GetCurrentProcessId();
        int buttonsHeld = (GetAsyncKeyState(0x01) | GetAsyncKeyState(0x02) | GetAsyncKeyState(0x04) | GetAsyncKeyState(0x05) | GetAsyncKeyState(0x06)) & 0x8000;
        if (inControl && sane) keepCursorInside();
        if (inControl && cfg.clipCursor && sane && !buttonsHeld && movementHeld()) {
            POINT p; GetCursorPos(&p);
            LONG cx = gameLeft + gameWidth / 2, cy = gameTop + gameHeight / 2;
            LONG dx = p.x - cx, dy = p.y - cy, lim = (gameWidth < gameHeight ? gameWidth : gameHeight) / 3;
            if (dx > lim || dx < -lim || dy > lim || dy < -lim) SetCursorPos(cx, cy);
        }
        // A menu opened from a key or button, or the cursor key: cursor to the middle.
        // (Moving the mouse into mouse mode leaves it where it is, unless CursorFromMiddle=1.)
        if (centerCursor) { centerCursor = 0; if (sane) SetCursorPos(gameLeft + gameWidth / 2, gameTop + gameHeight / 2); }
        // Cursor key pressed: a one-pixel real mouse move there and back, so the game
        // notices the mouse and shows its cursor.
        if (mouseMoveNudge) { mouseMoveNudge = 0; mouse_event(1 /*MOUSEEVENTF_MOVE*/, 1, 0, 0, 0); mouse_event(1, (DWORD)-1, 0, 0, 0); }
        return 0;
    }
    if (msg == 0x84) return -1;
    return DefWindowProcA(w, msg, wp, lp);
}

static DWORD legendThread(void *unused) {
    ClipCursor(0); // an older build could leave the mouse confined
    buildLegend();
    WNDCLASSA wc; memset(&wc, 0, sizeof(wc));
    wc.proc = legendProc; wc.instance = self; wc.className = "WasdModLegend";
    RegisterClassA(&wc);
    legendFont = makeFont(-14, 500);
    legendBrush = CreateSolidBrush(RGB(20, 22, 28));
    // Size the panel to its text.
    HANDLE dc = GetDC(0); SelectObject(dc, legendFont);
    int w = 160; SIZE s;
    if (textSize(dc, T("Keyboard controls"), &s) && s.cx > w) w = s.cx;
    for (int i = 0; i < legendCount; i++) if (textSize(dc, legendText[i], &s) && s.cx > w) w = s.cx;
    if (textSize(dc, "Ag", &s)) lineH = s.cy + 2;
    ReleaseDC(0, dc);
    legendW = w + 20; legendH = (legendCount + 1) * lineH + 20;
    legendWnd = CreateWindowExA(0x80000 | 0x20 | 0x8 | 0x80 | 0x8000000 /*LAYERED|TRANSPARENT|TOPMOST|TOOLWINDOW|NOACTIVATE*/,
                                "WasdModLegend", "Keyboard controls", 0x80000000 /*WS_POPUP*/, 0, 0, legendW, legendH, 0, 0, self, 0);
    if (!legendWnd) { logline("Could not create the key legend"); return 0; }
    SetLayeredWindowAttributes(legendWnd, 0, 215, 2 /*LWA_ALPHA*/);
    legendUntil = GetTickCount64() + (U64)cfg.legendSeconds * 1000;
    SetTimer(legendWnd, 1, 250, 0);
    {   // typing banner
        WNDCLASSA tc; memset(&tc, 0, sizeof(tc));
        tc.proc = typeProc; tc.instance = self; tc.className = "WasdModTyping";
        RegisterClassA(&tc);
        typeFont = makeFont(-22, 700);
        typeBrush = CreateSolidBrush(RGB(20, 22, 28)); typeEdge = CreateSolidBrush(RGB(255, 214, 102));
        HANDLE tdc = GetDC(0); SelectObject(tdc, typeFont); SIZE ts;
        if (!textSize(tdc, T("TYPING  -  Esc to finish"), &ts)) { ts.cx = 260; ts.cy = 26; }
        ReleaseDC(0, tdc);
        typeW = ts.cx + 40; typeH = ts.cy + 18;
        typeWnd = CreateWindowExA(0x80000 | 0x20 | 0x8 | 0x80 | 0x8000000 /*LAYERED|TRANSPARENT|TOPMOST|TOOLWINDOW|NOACTIVATE*/,
                                  "WasdModTyping", "Typing mode", 0x80000000 /*WS_POPUP*/, 0, 0, typeW, typeH, 0, 0, self, 0);
        if (typeWnd) SetLayeredWindowAttributes(typeWnd, 0, 230, 2 /*LWA_ALPHA*/);
    }
    WNDCLASSA cc; memset(&cc, 0, sizeof(cc));
    cc.proc = tickProc; cc.instance = self; cc.className = "WasdModTick";
    RegisterClassA(&cc);
    tickWnd = CreateWindowExA(0x80 | 0x8000000 /*TOOLWINDOW|NOACTIVATE*/, "WasdModTick", "WASD mod timer", 0x80000000 /*WS_POPUP, hidden*/, 0, 0, 1, 1, 0, 0, self, 0);
    if (tickWnd) SetTimer(tickWnd, 2, 30, 0);
    else logline("Could not create the timer window");
    MSG m;
    while (GetMessageA(&m, 0, 0, 0) > 0) DispatchMessageA(&m);
    return 0;
}

static void startLegend(void) {
    HANDLE t = CreateThread(0, 0, legendThread, 0, 0, 0);
    if (t) CloseHandle(t);
}
