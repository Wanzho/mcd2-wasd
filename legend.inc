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
IMP BOOL TextOutA(HANDLE, int, int, const char *, int);
IMP HANDLE CreateSolidBrush(DWORD);
IMP BOOL GetTextExtentPoint32A(HANDLE, const char *, int, SIZE *);
IMP BOOL SetCursorPos(int, int);
IMP BOOL ClipCursor(const RECT *);
IMP BOOL DeleteObject(HANDLE);

#define RGB(r, g, b) ((DWORD)(r) | ((DWORD)(g) << 8) | ((DWORD)(b) << 16))
#define LEGEND_LINES 32
static char legendText[LEGEND_LINES][96];
static int legendCount, legendW, legendH, lineH = 18;
static HANDLE legendWnd, legendFont, legendBrush;
static int legendShown;

static void addLine(const char *key, const char *what) {
    if (legendCount >= LEGEND_LINES) return;
    char *l = legendText[legendCount++]; l[0] = 0;
    append(l, 96, key); append(l, 96, "  ");
    append(l, 96, what);
}

// The game action a converted key presses, from the layout's [GameKeys] (the
// game's own keyboard keys, written by the key layout editor): S -> "Menu wheel".
static const char *gameActionFor(BYTE vk) {
    static const char *names[][2] = {
        {"wheel", "Menu wheel"}, {"inventory", "Inventory"}, {"map", "World map"}, {"quest", "Quests"},
        {"social", "Social menu"}, {"emotes", "Emotes"}, {"collect", "Collectibles"}, {"events", "Event log"},
        {"tpplayer", "Teleport to player"}, {"statue", "Teleport statue"}, {"tp1", "Teleport to player 1"},
        {"tp2", "Teleport to player 2"}, {"tp3", "Teleport to player 3"}, {"tp4", "Teleport to player 4"},
        {"ranged", "Bow"}, {"potion", "Health potion"}, {"jump", "Jump"}, {"melee", "Melee"},
        {"dodge", "Directional dodge"}, {"fdodge", "Forward dodge"}, {"art1", "Artifact 1"}, {"art2", "Artifact 2"},
        {"art3", "Artifact 3"}, {"trail", "Guidance trail"}, {"heavy", "Heavy jump attack"}, {"root", "Root"},
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
    addLine(move, "Move");
    if (dodgeMouse.text[0]) addLine(dodgeMouse.text, "Click: dodge. Click + drag: roll that way");
    for (int i = 0; i < NBTN; i++) {
        if (!btn[i].text[0]) continue;
        char what[64] = {0}; append(what, sizeof(what), btnLabel[i]);
        append(what, sizeof(what), "  ["); append(what, sizeof(what), btnName[i]); append(what, sizeof(what), "]");
        addLine(btn[i].text, what);
    }
    // Converted keys, named after what they do: "Tab  Menu wheel (sends the game's S)".
    for (int i = 0; i < remapCount; i++) {
        char what[96] = {0}; const char *name = gameActionFor(remapList[i].toVk);
        if (name) { append(what, sizeof(what), name); append(what, sizeof(what), " (sends the game's "); }
        else append(what, sizeof(what), "Sends the game's ");
        append(what, sizeof(what), remapList[i].to);
        if (name) append(what, sizeof(what), ")");
        addLine(remapList[i].from, what);
    }
    if (typeText[0]) addLine(typeText, "Type (Esc to stop)");
    if (cursorKey.text[0]) addLine(cursorKey.text, "Hold: mouse cursor");
    if (menuText[0]) addLine(menuText, "Game menus (mouse mode; move or fight to leave)");
    if (toggleKey.text[0]) addLine(toggleKey.text, "Keyboard & mouse on/off");
    if (legendKey.text[0]) addLine(legendKey.text, "Show / hide this list");
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
        TextOutA(dc, 10, 8, "Keyboard controls", 17);
        SetTextColor(dc, RGB(240, 240, 240));
        for (int i = 0; i < legendCount; i++) TextOutA(dc, 10, 8 + (i + 1) * lineH + 4, legendText[i], len(legendText[i]));
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
static const char TYPE_TEXT[] = "TYPING  -  Esc to finish";
static LRESULT typeProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        RECT r = {0, 0, typeW, typeH}; FillRect(dc, &r, typeEdge);
        RECT in = {3, 3, typeW - 3, typeH - 3}; FillRect(dc, &in, typeBrush);
        SelectObject(dc, typeFont); SetBkMode(dc, 1 /*TRANSPARENT*/); SetTextColor(dc, RGB(255, 214, 102));
        SIZE sz; GetTextExtentPoint32A(dc, TYPE_TEXT, sizeof(TYPE_TEXT) - 1, &sz);
        TextOutA(dc, (typeW - sz.cx) / 2, (typeH - sz.cy) / 2, TYPE_TEXT, sizeof(TYPE_TEXT) - 1);
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
    legendFont = CreateFontA(-14, 0, 0, 0, 500, 0, 0, 0, 0, 0, 0, 5 /*CLEARTYPE*/, 0, "Arial");
    legendBrush = CreateSolidBrush(RGB(20, 22, 28));
    // Size the panel to its text.
    HANDLE dc = GetDC(0); SelectObject(dc, legendFont);
    int w = 160; SIZE s;
    for (int i = 0; i < legendCount; i++) if (GetTextExtentPoint32A(dc, legendText[i], len(legendText[i]), &s) && s.cx > w) w = s.cx;
    if (GetTextExtentPoint32A(dc, "Ag", 2, &s)) lineH = s.cy + 2;
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
        typeFont = CreateFontA(-22, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 5 /*CLEARTYPE*/, 0, "Arial");
        typeBrush = CreateSolidBrush(RGB(20, 22, 28)); typeEdge = CreateSolidBrush(RGB(255, 214, 102));
        HANDLE tdc = GetDC(0); SelectObject(tdc, typeFont); SIZE ts;
        if (!GetTextExtentPoint32A(tdc, TYPE_TEXT, sizeof(TYPE_TEXT) - 1, &ts)) { ts.cx = 260; ts.cy = 26; }
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
