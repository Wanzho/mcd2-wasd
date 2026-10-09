// On-screen key list and banners (typing mode, mod off, layout loaded): click-through
// windows that never take focus, on their own thread with their own message loop, so
// they never block the game. The key list sits at the top-left of the game window. It
// is shown for LegendSeconds after startup and whenever the legend key turns it on.
//
// The look matches the wasdmod app and website: a dark glass panel, keys as keycaps
// tinted by group (movement, actions, menus, mod keys) and the controller button each
// one presses. It is drawn into a 32-bit picture with per-pixel alpha
// (UpdateLayeredWindow): shapes by the small anti-aliased rasteriser below (the same
// result on Windows and under Wine, whose GDI+ doesn't anti-alias shapes), text by GDI
// as a grey mask that is then coloured in. It is only redrawn when the layout, the
// language or the game window's size changes. If any of that fails, the old plain
// panel (GDI text on a translucent box) is used instead.

typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { UINT style; LRESULT (*proc)(HANDLE, UINT, WPARAM, LPARAM); int clsExtra, wndExtra; HANDLE instance, icon, cursor, background; const char *menu, *className; } WNDCLASSA;
typedef struct { HANDLE hdc; BOOL erase; RECT paint; BOOL restore, incUpdate; BYTE reserved[32]; } PAINTSTRUCT;
typedef struct { DWORD size; LONG width, height; WORD planes, bitCount; DWORD compression, sizeImage; LONG xPels, yPels; DWORD clrUsed, clrImportant; } BITMAPINFOHEADER;
typedef struct { BYTE op, flags, alpha, format; } BLENDFUNCTION;
typedef struct { LONG height, ascent, descent, internalLeading, externalLeading, aveCharWidth, maxCharWidth, weight, overhang, aspectX, aspectY; WCHAR first, last, fallback, brk; BYTE italic, underlined, struckOut, pitchFamily, charSet; } TEXTMETRICW;

IMP WORD RegisterClassA(const WNDCLASSA *);
IMP HANDLE CreateWindowExA(DWORD, const char *, const char *, DWORD, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
IMP LRESULT DefWindowProcA(HANDLE, UINT, WPARAM, LPARAM);
IMP BOOL ShowWindow(HANDLE, int);
IMP BOOL SetWindowPos(HANDLE, HANDLE, int, int, int, int, UINT);
IMP BOOL GetClientRect(HANDLE, RECT *);
IMP BOOL ClientToScreen(HANDLE, POINT *);
IMP BOOL SetLayeredWindowAttributes(HANDLE, DWORD, BYTE, DWORD);
IMP BOOL UpdateLayeredWindow(HANDLE, HANDLE, POINT *, SIZE *, HANDLE, POINT *, DWORD, const BLENDFUNCTION *, DWORD);
IMP BOOL GetMessageA(MSG *, HANDLE, UINT, UINT);
IMP LRESULT DispatchMessageA(const MSG *);
IMP U64 SetTimer(HANDLE, U64, UINT, void *);
IMP BOOL KillTimer(HANDLE, U64);
IMP BOOL InvalidateRect(HANDLE, const RECT *, BOOL);
IMP HANDLE BeginPaint(HANDLE, PAINTSTRUCT *);
IMP BOOL EndPaint(HANDLE, const PAINTSTRUCT *);
IMP int FillRect(HANDLE, const RECT *, HANDLE);
IMP BOOL IsWindowVisible(HANDLE);
IMP BOOL IsIconic(HANDLE);
IMP HANDLE GetDC(HANDLE);
IMP int ReleaseDC(HANDLE, HANDLE);
IMP HANDLE CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *);
IMP HANDLE SelectObject(HANDLE, HANDLE);
IMP DWORD SetTextColor(HANDLE, DWORD);
IMP int SetBkMode(HANDLE, int);
IMP BOOL TextOutW(HANDLE, int, int, const WCHAR *, int);
IMP HANDLE CreateSolidBrush(DWORD);
IMP BOOL GetTextExtentPoint32W(HANDLE, const WCHAR *, int, SIZE *);
IMP BOOL GetTextExtentExPointW(HANDLE, const WCHAR *, int, int, int *, int *, SIZE *);
IMP BOOL GetTextMetricsW(HANDLE, TEXTMETRICW *);
IMP int GetTextFaceA(HANDLE, int, char *);
IMP int GetDeviceCaps(HANDLE, int);
IMP HANDLE CreateCompatibleDC(HANDLE);
IMP BOOL DeleteDC(HANDLE);
IMP HANDLE CreateDIBSection(HANDLE, const BITMAPINFOHEADER *, UINT, void **, HANDLE, DWORD);
IMP BOOL GdiFlush(void);
IMP BOOL SetCursorPos(int, int);
IMP BOOL ClipCursor(const RECT *);
IMP BOOL DeleteObject(HANDLE);
IMP HANDLE GetStockObject(int);
IMP int SetTextCharacterExtra(HANDLE, int);
static int underWine; // the overlay thread runs under Wine (CrossOver, Proton): set when it starts
// Under Wine the overlays get even sizes and positions. With CrossOver's Retina mode a
// Windows pixel is half a Mac point, so an odd size or position puts the window's edge
// on half a point, and the screen pixels it leaves uncovered show as a white line
// along its right or bottom edge. The extra column or row is left transparent.
static int evenDown(int v) { return underWine ? v & ~1 : v; }
static int evenUp(int v) { return underWine ? (v + 1) & ~1 : v; }
IMP void Sleep(DWORD);
IMP UINT GetCaretBlinkTime(void);

#define RGB(r, g, b) ((DWORD)(r) | ((DWORD)(g) << 8) | ((DWORD)(b) << 16))
#define LEGEND_LINES 32
#define LINE_BYTES 200 // UTF-8, with room for the longer languages
static char legendText[LEGEND_LINES][LINE_BYTES]; // the plain panel's lines
static int legendCount, legendW, legendH, lineH = 18;
static HANDLE legendWnd, legendFont, legendBrush;
static int legendShown;

// The same list as rows: the keys, what they do, the controller button behind them
// and their group (the colours of the website's keyboard).
enum { CAT_MOVE, CAT_ACT, CAT_MENU, CAT_MOD };
#define PAD_NONE (-1)
#define PAD_LSTICK 100 // left stick: moving
#define PAD_RSTICK 101 // right stick: dodging
#define KEY_SEP '\x1F'   // between keys
#define GROUP_SEP '\x1E' // between first and second keys (W A S D, then Up Left Down Right)
typedef struct { char keys[96], text[LINE_BYTES]; int note, pad, cat; } LegendRow;
static LegendRow rows[LEGEND_LINES]; static int rowCount;
// Controller buttons in the order the app lists them, and their groups.
static const BYTE btnOrder[NBTN] = {B_LB, B_A, B_X, B_RT, B_Y, B_B, B_RB, B_LT, B_LS, B_DLEFT, B_DUP, B_BACK, B_START, B_DRIGHT, B_DDOWN, B_RS};
static const BYTE btnCat[NBTN] = {CAT_ACT, CAT_ACT, CAT_ACT, CAT_ACT, CAT_MOVE, CAT_ACT, CAT_ACT, CAT_ACT, CAT_MENU, CAT_MENU, CAT_ACT, CAT_MENU, CAT_MENU, CAT_MENU, CAT_ACT, CAT_MENU};

static void reloadIfChanged(void);
static void addLine(const char *key, const char *what) {
    if (legendCount >= LEGEND_LINES) return;
    char *l = legendText[legendCount++]; l[0] = 0;
    append(l, LINE_BYTES, key); append(l, LINE_BYTES, "  ");
    append(l, LINE_BYTES, what);
}
// Where a row's note starts ("Melee (in the air: ...)": at the bracket, ASCII or
// full-width); it is shown in the secondary colour. 0: no note.
static int noteStart(const char *t) {
    for (int i = 1; t[i]; i++) {
        if (t[i] == '(') return i;
        if ((BYTE)t[i] == 0xEF && (BYTE)t[i + 1] == 0xBC && (BYTE)t[i + 2] == 0x88) return i; // （
    }
    return 0;
}
static void addRow(const char *keys, const char *text, int pad, int cat) {
    if (rowCount >= LEGEND_LINES || !keys[0]) return;
    LegendRow *r = &rows[rowCount++];
    r->keys[0] = r->text[0] = 0;
    append(r->keys, sizeof(r->keys), keys); append(r->text, sizeof(r->text), text);
    r->note = noteStart(r->text); r->pad = pad; r->cat = cat;
}
// A binding's text ("Space / F") or a key list ("M G K Escape") as separate keys.
static void splitKeys(char *out, unsigned cap, const char *text, int slashes) {
    static const char sep[2] = {KEY_SEP, 0};
    out[0] = 0;
    for (const char *p = text; *p; ) {
        if (slashes ? startsWith(p, " / ") : *p == ' ') {
            p += slashes ? 3 : 1;
            if (out[0] && out[len(out) - 1] != KEY_SEP) append(out, cap, sep);
            continue;
        }
        char c[2] = {*p++, 0}; append(out, cap, c);
    }
}
// The n-th key of a binding ("W / Up", 1 -> "Up"), or "".
static void nthKey(const char *text, int n, char *out, unsigned cap) {
    char all[96]; splitKeys(all, sizeof(all), text, 1);
    const char *p = all;
    for (int i = 0; i < n && *p; i++) { while (*p && *p != KEY_SEP) p++; if (*p) p++; }
    out[0] = 0;
    while (*p && *p != KEY_SEP) { char c[2] = {*p++, 0}; append(out, cap, c); }
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
// on a Mac and on Linux), Arial for the rest. (The plain panel's font.)
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
// (in English; the key list translates it), and its group.
static const char *gameActionFor(BYTE vk, int *cat) {
    static const struct { const char *code, *name; BYTE cat; } names[] = {
        {"wheel", N_("Menu wheel"), CAT_MENU}, {"inventory", N_("Inventory"), CAT_MENU}, {"map", N_("World map"), CAT_MENU}, {"quest", N_("Quests"), CAT_MENU},
        {"social", N_("Social menu"), CAT_MENU}, {"emotes", N_("Emotes"), CAT_MENU}, {"collect", N_("Collectibles"), CAT_MENU}, {"events", N_("Event log"), CAT_MENU},
        {"tpplayer", N_("Teleport to player"), CAT_ACT}, {"statue", N_("Teleport statue"), CAT_ACT}, {"tp1", N_("Teleport to player 1"), CAT_ACT},
        {"tp2", N_("Teleport to player 2"), CAT_ACT}, {"tp3", N_("Teleport to player 3"), CAT_ACT}, {"tp4", N_("Teleport to player 4"), CAT_ACT},
        {"ranged", N_("Bow"), CAT_ACT}, {"potion", N_("Health potion"), CAT_ACT}, {"jump", N_("Jump"), CAT_ACT}, {"melee", N_("Melee"), CAT_ACT},
        {"dodge", N_("Directional dodge"), CAT_MOVE}, {"fdodge", N_("Forward dodge"), CAT_MOVE}, {"art1", N_("Artifact 1"), CAT_ACT}, {"art2", N_("Artifact 2"), CAT_ACT},
        {"art3", N_("Artifact 3"), CAT_ACT}, {"trail", N_("Guidance trail"), CAT_ACT}, {"heavy", N_("Heavy jump attack"), CAT_ACT}, {"root", N_("Root"), CAT_MOVE},
    };
    static char sec[2048];
    *cat = CAT_MENU;
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
                const char *a = p, *n = names[i].code;
                while (a < eq && *n && lower(*a) == *n) a++, n++;
                if (a == eq && !*n) { *cat = names[i].cat; return names[i].name; }
            }
        }
    }
    return 0;
}

static void buildLegend(void) {
    static const char keySep[2] = {KEY_SEP, 0}, groupSep[2] = {GROUP_SEP, 0};
    legendCount = rowCount = 0;
    char move[64] = {0}, keys[96], k[32];
    append(move, sizeof(move), moveUp.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveLeft.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveDown.text); append(move, sizeof(move), " ");
    append(move, sizeof(move), moveRight.text);
    addLine(move, T("Move"));
    // Moving: the first key of each direction, then the second ones as another group.
    keys[0] = 0;
    for (int g = 0; g < MAXKEYS; g++) {
        const Binding *dirs[4] = {&moveUp, &moveLeft, &moveDown, &moveRight};
        char group[96] = {0};
        for (int d = 0; d < 4; d++) {
            nthKey(dirs[d]->text, g, k, sizeof(k));
            if (!k[0]) continue;
            if (group[0]) append(group, sizeof(group), keySep);
            append(group, sizeof(group), k);
        }
        if (!group[0]) continue;
        if (keys[0]) append(keys, sizeof(keys), groupSep);
        append(keys, sizeof(keys), group);
    }
    addRow(keys, T("Move"), PAD_LSTICK, CAT_MOVE);
    if (dodgeMouse.text[0]) {
        addLine(dodgeMouse.text, T("Click: dodge. Click + drag: roll that way"));
        splitKeys(keys, sizeof(keys), dodgeMouse.text, 1);
        addRow(keys, T("Click: dodge. Click + drag: roll that way"), PAD_RSTICK, CAT_MOVE);
    }
    for (int i = 0; i < NBTN; i++) {
        if (!btn[i].text[0]) continue;
        char what[LINE_BYTES] = {0}; append(what, sizeof(what), T(btnLabel[i]));
        append(what, sizeof(what), "  ["); append(what, sizeof(what), btnName[i]); append(what, sizeof(what), "]");
        addLine(btn[i].text, what);
    }
    for (int j = 0; j < NBTN; j++) {
        int i = btnOrder[j];
        if (!btn[i].text[0]) continue;
        splitKeys(keys, sizeof(keys), btn[i].text, 1);
        addRow(keys, T(btnLabel[i]), i, btnCat[i]);
    }
    // Converted keys, named after what they do: "Tab  Menu wheel (sends the game's S)".
    for (int i = 0; i < remapCount; i++) {
        char what[LINE_BYTES]; int cat; const char *name = gameActionFor(remapList[i].toVk, &cat);
        if (name) fill(what, sizeof(what), T("{action} (sends the game's {key})"), T(name), remapList[i].to);
        else fill(what, sizeof(what), T("Sends the game's {key}"), "", remapList[i].to);
        addLine(remapList[i].from, what);
        addRow(remapList[i].from, what, PAD_NONE, cat);
    }
    if (typeText[0]) { addLine(typeText, T("Type (Esc to stop)")); splitKeys(keys, sizeof(keys), typeText, 0); addRow(keys, T("Type (Esc to stop)"), PAD_NONE, CAT_MOD); }
    if (cursorKey.text[0]) {
        const char *what = cfg.cursorToggle ? T("Press: mouse cursor on / off") : T("Hold: mouse cursor");
        addLine(cursorKey.text, what); splitKeys(keys, sizeof(keys), cursorKey.text, 1); addRow(keys, what, PAD_NONE, CAT_MOD);
    }
    if (menuText[0]) {
        addLine(menuText, T("Game menus (mouse mode; move or fight to leave)"));
        splitKeys(keys, sizeof(keys), menuText, 0); addRow(keys, T("Game menus (mouse mode; move or fight to leave)"), PAD_NONE, CAT_MENU);
    }
    if (toggleKey.text[0]) { addLine(toggleKey.text, T("Keyboard & mouse on/off")); splitKeys(keys, sizeof(keys), toggleKey.text, 1); addRow(keys, T("Keyboard & mouse on/off"), PAD_NONE, CAT_MOD); }
    if (legendKey.text[0]) { addLine(legendKey.text, T("Show / hide this list")); splitKeys(keys, sizeof(keys), legendKey.text, 1); addRow(keys, T("Show / hide this list"), PAD_NONE, CAT_MOD); }
    // Grouped (movement, actions, menus, mod keys), keeping the order within a group.
    for (int i = 1; i < rowCount; i++) {
        LegendRow r = rows[i]; int j = i;
        while (j > 0 && rows[j - 1].cat > r.cat) { rows[j] = rows[j - 1]; j--; }
        rows[j] = r;
    }
}

// ---------------------------------------------------------------- drawing
// A 32-bit top-down picture with premultiplied alpha (what UpdateLayeredWindow takes),
// selected into its own DC.
typedef struct { int w, h; DWORD *px; HANDLE dc, bmp, old; } Surf;
static void surfFree(Surf *s) {
    if (s->dc) { if (s->old) SelectObject(s->dc, s->old); DeleteDC(s->dc); }
    if (s->bmp) DeleteObject(s->bmp);
    memset(s, 0, sizeof(*s));
}
static int surfMake(Surf *s, int w, int h) {
    if (s->px && s->w == w && s->h == h) return 1;
    surfFree(s);
    if (w < 1 || h < 1 || w > 8192 || h > 8192) return 0;
    BITMAPINFOHEADER bi; memset(&bi, 0, sizeof(bi));
    bi.size = sizeof(bi); bi.width = w; bi.height = -h; bi.planes = 1; bi.bitCount = 32;
    void *bits = 0;
    s->dc = CreateCompatibleDC(0);
    s->bmp = s->dc ? CreateDIBSection(s->dc, &bi, 0 /*DIB_RGB_COLORS*/, &bits, 0, 0) : 0;
    if (!s->bmp || !bits) { surfFree(s); return 0; }
    s->old = SelectObject(s->dc, s->bmp); s->px = bits; s->w = w; s->h = h;
    return 1;
}
static void surfClear(Surf *s) { DWORD *p = s->px; for (long n = (long)s->w * s->h; n > 0; n--) *p++ = 0; }
// Shows a picture as a layered window's content, at screen position (x, y), with an
// overall alpha on top of its own.
static int pushLayer(HANDLE w, Surf *s, int x, int y, int alpha) {
    POINT dst = {x, y}, src = {0, 0}; SIZE sz = {s->w, s->h};
    BLENDFUNCTION bf = {0 /*AC_SRC_OVER*/, 0, (BYTE)(alpha < 0 ? 0 : alpha > 255 ? 255 : alpha), 1 /*AC_SRC_ALPHA*/};
    return s->px && UpdateLayeredWindow(w, 0, &dst, &sz, s->dc, &src, 0, &bf, 2 /*ULW_ALPHA*/);
}
// Whether a layered window takes per-pixel alpha (tried with an empty 1x1 picture).
static int canLayer(HANDLE w) {
    Surf t; memset(&t, 0, sizeof(t));
    if (!w || !surfMake(&t, 2, 2)) return 0;
    surfClear(&t);
    int ok = pushLayer(w, &t, 0, 0, 0);
    surfFree(&t);
    return ok;
}

static float fsqrt(float v) { if (v <= 0) return 0; float r; __asm__("sqrtss %1, %0" : "=x"(r) : "x"(v)); return r; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static int ifloor(float v) { int i = (int)v; return v < (float)i ? i - 1 : i; }
static int iround(float v) { return ifloor(v + 0.5f); }
static float smooth01(float t) { t = clamp01(t); return t * t * (3 - 2 * t); }

// Colours are 0xRRGGBB.
#define C_TEXT 0xF5F5F7
#define C_SUB 0xA1A1A6
#define C_AMBER 0xFFB340
#define C_ORANGE 0xFF6A3D
#define C_PINK 0xFF3D77
static const DWORD catColor[4] = {0x5BB5E0, 0xF19A2E, 0x6CC27F, 0xA68CF0};
static const DWORD faceColor[4] = {0x5CBF5C, 0xE0584F, 0x4F8FE0, 0xE8B83A}; // A, B, X, Y
static DWORD mix(DWORD a, DWORD b, float t) {
    t = clamp01(t);
    int ar = (int)(a >> 16) & 255, ag = (int)(a >> 8) & 255, ab = (int)a & 255;
    int br = (int)(b >> 16) & 255, bg = (int)(b >> 8) & 255, bb = (int)b & 255;
    return (DWORD)(ar + (int)((br - ar) * t)) << 16 | (DWORD)(ag + (int)((bg - ag) * t)) << 8 | (DWORD)(ab + (int)((bb - ab) * t));
}
// Paints colour c with alpha a (0-255) over a premultiplied pixel.
static void over(DWORD *p, DWORD c, int a) {
    if (a <= 0) return;
    if (a > 255) a = 255;
    DWORD d = *p; int k = 255 - a;
    int r = (int)((c >> 16) & 255) * a + (int)((d >> 16) & 255) * k;
    int g = (int)((c >> 8) & 255) * a + (int)((d >> 8) & 255) * k;
    int b = (int)(c & 255) * a + (int)(d & 255) * k;
    int na = a * 255 + (int)(d >> 24) * k;
    *p = (DWORD)((na + 127) / 255) << 24 | (DWORD)((r + 127) / 255) << 16 | (DWORD)((g + 127) / 255) << 8 | (DWORD)((b + 127) / 255);
}
// A colour, or a gradient from c1/a1 to c2/a2: top to bottom, or left to right (across).
typedef struct { DWORD c1, c2; int a1, a2, across; } Ink;
static Ink ink(DWORD c, int a) { Ink k = {c, c, a, a, 0}; return k; }
static Ink grad(DWORD c1, int a1, DWORD c2, int a2, int across) { Ink k = {c1, c2, a1, a2, across}; return k; }

// Distance from (x, y) to the edge of a rounded rectangle (negative inside).
static float sdBox(float x, float y, float l, float t, float r, float b, float rad) {
    float hw = (r - l) * 0.5f, hh = (b - t) * 0.5f;
    if (rad > hw) rad = hw;
    if (rad > hh) rad = hh;
    float qx = x - (l + r) * 0.5f, qy = y - (t + b) * 0.5f;
    if (qx < 0) qx = -qx;
    if (qy < 0) qy = -qy;
    qx -= hw - rad; qy -= hh - rad;
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, in = qx > qy ? qx : qy;
    return fsqrt(ox * ox + oy * oy) + (in < 0 ? in : 0) - rad;
}
enum { BOX_FILL, BOX_LINE, BOX_GLOW };
// A rounded rectangle: filled, outlined w pixels wide just inside its edge, or a soft
// glow (or shadow) reaching w pixels out from its edge.
static void box(Surf *s, float l, float t, float r, float b, float rad, int mode, float w, Ink k) {
    float grow = mode == BOX_GLOW ? w : 0;
    int x0 = ifloor(l - grow) - 1, y0 = ifloor(t - grow) - 1, x1 = ifloor(r + grow) + 2, y1 = ifloor(b + grow) + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    float bw = r - l > 0 ? r - l : 1, bh = b - t > 0 ? b - t : 1;
    for (int y = y0; y < y1; y++) {
        float py = y + 0.5f, fy = clamp01((py - t) / bh);
        DWORD *row = s->px + (long)y * s->w;
        for (int x = x0; x < x1; x++) {
            float px = x + 0.5f, d = sdBox(px, py, l, t, r, b, rad), cov;
            if (mode == BOX_FILL) cov = clamp01(0.5f - d);
            else if (mode == BOX_LINE) cov = clamp01(0.5f - d) - clamp01(0.5f - d - w);
            else cov = 1 - smooth01((d + w) / (2 * w));
            if (cov <= 0.003f) continue;
            float f = k.across ? clamp01((px - l) / bw) : fy;
            int a = k.a1 + (int)((k.a2 - k.a1) * f);
            over(row + x, k.c1 == k.c2 ? k.c1 : mix(k.c1, k.c2, f), (int)(a * cov + 0.5f));
        }
    }
}
// A filled triangle (icons).
static void tri(Surf *s, float ax, float ay, float bx, float by, float cx, float cy, DWORD c, int a) {
    float xs[3] = {ax, bx, cx}, ys[3] = {ay, by, cy}, nx[3], ny[3], off[3];
    float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax), sgn = area < 0 ? -1.0f : 1.0f;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        float ex = xs[j] - xs[i], ey = ys[j] - ys[i], l = fsqrt(ex * ex + ey * ey);
        if (l <= 0) return;
        nx[i] = sgn * ey / l; ny[i] = -sgn * ex / l; off[i] = nx[i] * xs[i] + ny[i] * ys[i];
    }
    float minx = ax < bx ? ax : bx, maxx = ax > bx ? ax : bx, miny = ay < by ? ay : by, maxy = ay > by ? ay : by;
    if (cx < minx) minx = cx;
    if (cx > maxx) maxx = cx;
    if (cy < miny) miny = cy;
    if (cy > maxy) maxy = cy;
    int x0 = ifloor(minx) - 1, x1 = ifloor(maxx) + 2, y0 = ifloor(miny) - 1, y1 = ifloor(maxy) + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        float px = x + 0.5f, py = y + 0.5f, d = -1e9f;
        for (int i = 0; i < 3; i++) { float e = nx[i] * px + ny[i] * py - off[i]; if (e > d) d = e; }
        float cov = clamp01(0.5f - d);
        if (cov > 0.003f) over(s->px + (long)y * s->w + x, c, (int)(a * cov + 0.5f));
    }
}

// A line with round ends from (ax, ay) to (bx, by), r thick on each side (icons).
static void capsule(Surf *s, float ax, float ay, float bx, float by, float r, DWORD c, int a) {
    float dx = bx - ax, dy = by - ay, ll = dx * dx + dy * dy;
    int x0 = ifloor((ax < bx ? ax : bx) - r) - 1, x1 = ifloor((ax > bx ? ax : bx) + r) + 2;
    int y0 = ifloor((ay < by ? ay : by) - r) - 1, y1 = ifloor((ay > by ? ay : by) + r) + 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s->w) x1 = s->w;
    if (y1 > s->h) y1 = s->h;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        float px = x + 0.5f - ax, py = y + 0.5f - ay, t = ll > 0 ? clamp01((px * dx + py * dy) / ll) : 0;
        float ex = px - t * dx, ey = py - t * dy, cov = clamp01(0.5f - (fsqrt(ex * ex + ey * ey) - r));
        if (cov > 0.003f) over(s->px + (long)y * s->w + x, c, (int)(a * cov + 0.5f));
    }
}

// Text: GDI draws it white on black into a scratch picture (grey anti-aliasing), and
// that is used as coverage for the colour, so it gets real alpha.
static Surf maskSurf;
static HANDLE measureDC;
// Before fonts are deleted: none of them may still be selected into these DCs
// (DeleteObject refuses a selected font, and it would stay for the rest of the game).
static void unselectFonts(void) {
    HANDLE f = GetStockObject(17 /*DEFAULT_GUI_FONT*/);
    if (measureDC) SelectObject(measureDC, f);
    if (maskSurf.dc) SelectObject(maskSurf.dc, f);
}
// Keycap labels get a pixel of extra spacing: at small sizes Segoe UI Bold's "E" runs
// into the "s" of "Esc". Measuring and drawing both go through here, so widths match.
static HANDLE spacedFont[3];
static void useFont(HANDLE dc, HANDLE f) {
    SelectObject(dc, f);
    SetTextCharacterExtra(dc, f && (f == spacedFont[0] || f == spacedFont[1] || f == spacedFont[2]));
}
static int textWidth(HANDLE f, const WCHAR *w, int n) {
    SIZE sz; if (n <= 0 || !measureDC) return 0;
    useFont(measureDC, f);
    return GetTextExtentPoint32W(measureDC, w, n, &sz) ? sz.cx : 0;
}
static void fontMetrics(HANDLE f, int *ascent, int *height) {
    TEXTMETRICW tm; *ascent = 12; *height = 16;
    if (!measureDC) return;
    useFont(measureDC, f);
    if (GetTextMetricsW(measureDC, &tm)) { *ascent = tm.ascent; *height = tm.height; }
}
// Draws text with its top-left at (x, y); a gradient runs from x = g0 to g1.
static int drawText(Surf *s, HANDLE f, int x, int y, const WCHAR *w, int n, Ink k, float g0, float g1) {
    SIZE sz;
    if (n <= 0 || !maskSurf.dc) return 0;
    useFont(maskSurf.dc, f);
    if (!GetTextExtentPoint32W(maskSurf.dc, w, n, &sz)) return 0;
    int l = x - 4, t = y - 2, r = x + sz.cx + 6, b = y + sz.cy + 3;
    if (l < 0) l = 0;
    if (t < 0) t = 0;
    if (r > s->w) r = s->w;
    if (b > s->h) b = s->h;
    if (r > maskSurf.w) r = maskSurf.w;
    if (b > maskSurf.h) b = maskSurf.h;
    if (l >= r || t >= b) return sz.cx;
    for (int yy = t; yy < b; yy++) for (int xx = l; xx < r; xx++) maskSurf.px[(long)yy * maskSurf.w + xx] = 0;
    SetBkMode(maskSurf.dc, 1 /*TRANSPARENT*/); SetTextColor(maskSurf.dc, 0xFFFFFF);
    TextOutW(maskSurf.dc, x, y, w, n);
    GdiFlush();
    for (int yy = t; yy < b; yy++) for (int xx = l; xx < r; xx++) {
        DWORD m = maskSurf.px[(long)yy * maskSurf.w + xx];
        int cov = (int)(m >> 8) & 255, cr = (int)(m >> 16) & 255, cb = (int)m & 255;
        if (cr > cov) cov = cr; // ClearType, if the system forces it: the strongest channel
        if (cb > cov) cov = cb;
        if (!cov) continue;
        float fx = g1 > g0 ? clamp01((xx + 0.5f - g0) / (g1 - g0)) : 0;
        int a = k.a1 + (int)((k.a2 - k.a1) * fx);
        over(s->px + (long)yy * s->w + xx, k.c1 == k.c2 ? k.c1 : mix(k.c1, k.c2, fx), a * cov / 255);
    }
    return sz.cx;
}

// ---------------------------------------------------------------- fonts (new look)
// Grey anti-aliased fonts. The language's own font for its text; key names and button
// letters always in a clean Latin face. Each list goes from Windows' fonts to a Mac's
// (under CrossOver) to Linux's; the first one installed is used. A name starting with
// '!' is a single weight already (Hiragino Sans W6), asked for at normal weight so it
// isn't made bolder still.
// The face found is remembered (per list), so other sizes don't look again.
static const char *const *faceListSeen[8]; static const char *faceFound[8]; static int faceSeen;
static HANDLE faceFont(int px, int weight, DWORD charset, const char *const *faces, int n) {
    HANDLE found = 0, dc = 0;
    for (int f = 0; f < n && !found; f++) {
        const char *face = faces[f];
        if (!face) continue;
        int known = -1;
        for (int i = 0; i < faceSeen; i++) if (faceListSeen[i] == faces) known = i;
        if (known >= 0 && faceFound[known] && faceFound[known] != face) continue; // a face earlier in the list wasn't there
        int w = weight;
        if (*face == '!') { face++; w = 400; }
        HANDLE font = CreateFontA(-px, 0, 0, 0, w, 0, 0, 0, charset, 0, 0, 4 /*ANTIALIASED_QUALITY*/, 0, face);
        if (!font) continue;
        if (known >= 0 && faceFound[known]) { found = font; break; }
        if (!dc) dc = GetDC(0);
        char got[64] = {0}; HANDLE old = SelectObject(dc, font); GetTextFaceA(dc, sizeof(got), got); SelectObject(dc, old);
        int k = 0; while (got[k] && lower(got[k]) == lower(face[k])) k++;
        if (!got[k] && !face[k]) {
            found = font;
            if (known < 0 && faceSeen < 8) { faceListSeen[faceSeen] = faces; faceFound[faceSeen++] = faces[f]; }
        } else DeleteObject(font);
    }
    if (dc) ReleaseDC(0, dc);
    return found;
}
static HANDLE uiFont(int px, int weight, int latin) {
    int bold = weight >= 600;
    if (px < 6) px = 6;
    if (!latin) {
        const char *code = LANG_CODE[langIndex];
        static const char *const ja[] = {"Meiryo UI", "Yu Gothic UI", "!Hiragino Sans W4", "Hiragino Sans", "Hiragino Kaku Gothic ProN", "!YuGothic Medium", "Noto Sans CJK JP", "Source Han Sans"};
        static const char *const jaBold[] = {"Meiryo UI", "Yu Gothic UI", "!Hiragino Sans W6", "Hiragino Sans", "Hiragino Kaku Gothic ProN", "!YuGothic Bold", "Noto Sans CJK JP", "Source Han Sans"};
        static const char *const hans[] = {"Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Hiragino Sans GB", "Noto Sans CJK SC", "Source Han Sans SC"};
        static const char *const hant[] = {"Microsoft JhengHei UI", "Microsoft JhengHei", "PingFang TC", "Heiti TC", "Noto Sans CJK TC", "Source Han Sans TC"};
        static const char *const ko[] = {"Malgun Gothic", "Apple SD Gothic Neo", "AppleGothic", "Noto Sans CJK KR", "Source Han Sans K"};
        const char *const *faces = 0; int n = 0; DWORD charset = 1;
        if (sameStr(code, "ja")) { faces = bold ? jaBold : ja; n = 8; charset = 128; }
        else if (sameStr(code, "zh-Hans")) { faces = hans; n = 6; charset = 134; }
        else if (sameStr(code, "zh-Hant")) { faces = hant; n = 6; charset = 136; }
        else if (sameStr(code, "ko")) { faces = ko; n = 5; charset = 129; }
        if (faces) {
            HANDLE f = faceFont(px, weight, charset, faces, n);
            return f ? f : CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, charset, 0, 0, 4, 0, ""); // any font with these letters
        }
    }
    static const char *const latinFaces[] = {"Segoe UI", "Helvetica Neue", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Arial"};
    static const char *const latinSemibold[] = {"Segoe UI Semibold", "Segoe UI", "Helvetica Neue", "Noto Sans", "DejaVu Sans", "Liberation Sans", "Arial"};
    HANDLE f = weight == 600 ? faceFont(px, weight, 1, latinSemibold, 7) : faceFont(px, weight, 1 /*DEFAULT_CHARSET*/, latinFaces, 6);
    return f ? f : CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, 1, 0, 0, 4, 0, "Arial");
}

// ---------------------------------------------------------------- the key list
// Sizes at scale 1 (1080p, 96 DPI); everything is multiplied by the scale.
static struct {
    float s; int lang;
    HANDLE title, text, note, key, keyWord, btn, btnSmall;
    int textAsc, textH, noteAsc, noteH, titleH, keyAsc, wordAsc, btnAsc, smallAsc;
    int capH, capGap, capPad, rowGap, groupGap, pad, padTop, colGap, gapKT, gapTB, radius, margin, badge, keyMax, maxText, headerGap;
} look;
static int px(float v) { return iround(v * look.s); }
static void lookFor(float s) {
    if (look.title && look.s == s && look.lang == langIndex) return;
    HANDLE *fonts[] = {&look.title, &look.text, &look.note, &look.key, &look.keyWord, &look.btn, &look.btnSmall};
    unselectFonts();
    for (unsigned i = 0; i < sizeof(fonts) / sizeof(*fonts); i++) if (*fonts[i]) { DeleteObject(*fonts[i]); *fonts[i] = 0; }
    look.s = s; look.lang = langIndex;
    look.title = uiFont(px(15), 700, 0); look.text = uiFont(px(14), 500, 0); look.note = uiFont(px(12.5f), 400, 0);
    look.key = uiFont(px(12), 600, 1); look.keyWord = uiFont(px(11), 600, 1);
    look.btn = uiFont(px(11), 700, 1); look.btnSmall = uiFont(px(9.5f), 700, 1);
    spacedFont[0] = look.btn; spacedFont[1] = look.btnSmall;
    int a, h;
    fontMetrics(look.title, &a, &look.titleH);
    fontMetrics(look.text, &look.textAsc, &look.textH); fontMetrics(look.note, &look.noteAsc, &look.noteH);
    fontMetrics(look.key, &look.keyAsc, &h); fontMetrics(look.keyWord, &look.wordAsc, &h);
    fontMetrics(look.btn, &look.btnAsc, &h); fontMetrics(look.btnSmall, &look.smallAsc, &h);
    look.capH = px(22); look.capGap = px(4); look.capPad = px(7); look.rowGap = px(6); look.groupGap = px(9);
    look.pad = px(16); look.padTop = px(14); look.colGap = px(30); look.gapKT = px(10); look.gapTB = px(12);
    look.radius = px(14); look.margin = px(22); look.badge = px(20); look.headerGap = px(12);
    look.keyMax = 4 * look.capH + 3 * look.capGap; look.maxText = px(250);
}
// Text centred in a box of height h at y: where to put the top of its line.
static int centreText(int y, int h, int ascent, int px) { return y + h / 2 + iround(px * 0.36f) - ascent; }

// Keycaps: the label (or icon) each key shows.
enum { IC_NONE, IC_MOUSE1, IC_MOUSE2, IC_MOUSE3, IC_MOUSE4, IC_MOUSE5, IC_UP, IC_DOWN, IC_LEFT, IC_RIGHT, IC_TILDE };
typedef struct { WCHAR label[24]; int n, icon, group, w; } Cap;
static int capLabel(const char *name, WCHAR *out, int *n) {
    static const struct { const char *name; int icon; const char *label; } special[] = {
        {"mouse1", IC_MOUSE1, ""}, {"mouse2", IC_MOUSE2, ""}, {"mouse3", IC_MOUSE3, ""}, {"mouse4", IC_MOUSE4, "4"}, {"mouse5", IC_MOUSE5, "5"},
        {"up", IC_UP, ""}, {"down", IC_DOWN, ""}, {"left", IC_LEFT, ""}, {"right", IC_RIGHT, ""},
        {"escape", IC_NONE, "Esc"}, {"esc", IC_NONE, "Esc"}, {"backtick", IC_TILDE, ""}, {"grave", IC_TILDE, ""}, {"tilde", IC_TILDE, ""}, {"`", IC_TILDE, ""},
    };
    for (unsigned i = 0; i < sizeof(special) / sizeof(*special); i++)
        if (named(name, special[i].name)) { *n = widen(special[i].label, out, 24); return special[i].icon; }
    char t[32] = {0}; append(t, sizeof(t), name);
    if (t[0] >= 'a' && t[0] <= 'z') t[0] -= 32; // "w" -> "W", "space" -> "Space"
    *n = widen(t, out, 24);
    return IC_NONE;
}
static int capWidth(const Cap *c) {
    if (c->icon == IC_MOUSE4 || c->icon == IC_MOUSE5) return look.capH + px(8);
    if (c->icon != IC_NONE) return look.capH;
    int tw = textWidth(c->n == 1 ? look.key : look.keyWord, c->label, c->n);
    if (c->n == 1 && tw <= look.capH - px(8)) return look.capH; // a letter: a square key
    return tw + 2 * look.capPad > look.capH ? tw + 2 * look.capPad : look.capH;
}
static int rowCaps(const LegendRow *r, Cap *caps, int max) {
    int count = 0, group = 0, ni = 0; char name[32];
    for (const char *p = r->keys; ; p++) {
        if (*p == KEY_SEP || *p == GROUP_SEP || !*p) {
            if (ni && count < max) {
                name[ni] = 0; Cap *c = &caps[count++];
                c->group = group; c->icon = capLabel(name, c->label, &c->n); c->w = capWidth(c);
            }
            ni = 0;
            if (*p == GROUP_SEP) group++;
            if (!*p) break;
            continue;
        }
        if (ni < 31) name[ni++] = *p;
    }
    return count;
}
// Keycaps in lines at most keyW wide (a long list, like the game's menu keys, takes
// several lines). first[l] is the first cap of line l; returns the number of lines.
static int capLines(const Cap *c, int n, int keyW, int *first, int *width) {
    int lines = 0, x = 0;
    for (int i = 0; i < n; i++) {
        int need = c[i].w + (x ? look.capGap : 0);
        if (!x || x + need > keyW) {
            if (lines >= 8) break;
            first[lines] = i; width[lines] = c[i].w; x = c[i].w; lines++;
        } else { x += need; width[lines - 1] = x; }
    }
    first[lines] = n;
    return lines;
}

static void drawMouse(Surf *s, float cx, float cy, int button, DWORD hot) {
    float u = look.s, l = cx - 5.5f * u, r = cx + 5.5f * u, t = cy - 7.5f * u, b = cy + 7.5f * u, split = t + 6.2f * u;
    // The pressed button lit, then the body's outline and the button seams.
    if (button == 1 || button == 2) {
        for (int y = ifloor(t); y < ifloor(split) + 1; y++) for (int x = ifloor(l); x < ifloor(r) + 1; x++) {
            if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
            float px = x + 0.5f, py = y + 0.5f;
            float cov = clamp01(0.5f - sdBox(px, py, l, t, r, b, 5.5f * u)) * clamp01(split - py + 0.5f);
            cov *= button == 1 ? clamp01(cx - px + 0.5f) : clamp01(px - cx + 0.5f);
            if (cov > 0.003f) over(s->px + (long)y * s->w + x, hot, (int)(255 * cov));
        }
    }
    box(s, l, t, r, b, 5.5f * u, BOX_LINE, 1.15f * u, ink(0xEDEDF0, 240));
    box(s, cx - 0.5f * u, t + 0.6f * u, cx + 0.5f * u, split, 0.4f * u, BOX_FILL, 0, ink(0xEDEDF0, 220));
    box(s, l + 0.6f * u, split - 0.5f * u, r - 0.6f * u, split + 0.5f * u, 0.4f * u, BOX_FILL, 0, ink(0xEDEDF0, 220));
    if (button == 3) box(s, cx - 1.5f * u, t + 1.6f * u, cx + 1.5f * u, t + 6.0f * u, 1.5f * u, BOX_FILL, 0, ink(hot, 255));
    if (button == 4 || button == 5) { // side buttons on the left: front (5) above back (4)
        float y5 = t + 5.4f * u, y4 = t + 9.6f * u, h = 3.4f * u;
        box(s, l - 2.4f * u, y5, l + 0.6f * u, y5 + h, 1.2f * u, BOX_FILL, 0, button == 5 ? ink(hot, 255) : ink(0x8A8A90, 200));
        box(s, l - 2.4f * u, y4, l + 0.6f * u, y4 + h, 1.2f * u, BOX_FILL, 0, button == 4 ? ink(hot, 255) : ink(0x8A8A90, 200));
    }
}
static void drawCap(Surf *s, float x, float y, const Cap *c, int cat) {
    float w = (float)c->w, h = (float)look.capH, rad = 5 * look.s, u = look.s;
    DWORD tint = catColor[cat];
    box(s, x, y + 1.5f * u, x + w, y + h + 1.5f * u, rad, BOX_FILL, 0, ink(0x000000, 150)); // the key's lower edge
    box(s, x, y, x + w, y + h, rad, BOX_FILL, 0, grad(mix(0x323237, tint, 0.20f), 255, mix(0x1F1F23, tint, 0.13f), 255, 0));
    box(s, x, y, x + w, y + h, rad, BOX_LINE, 1.0f * u, grad(mix(0x6A6A72, tint, 0.65f), 210, mix(0x34343A, tint, 0.45f), 190, 0));
    box(s, x + rad, y + 1.0f * u, x + w - rad, y + 2.0f * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 26)); // top highlight
    float cx = x + w / 2, cy = y + h / 2;
    switch (c->icon) {
    case IC_MOUSE1: case IC_MOUSE2: case IC_MOUSE3: drawMouse(s, cx, cy, c->icon - IC_MOUSE1 + 1, tint); break;
    case IC_MOUSE4: case IC_MOUSE5: {
        drawMouse(s, x + 3.0f * u + 5.5f * u + 3.0f * u, cy, c->icon - IC_MOUSE1 + 1, tint);
        int tw = textWidth(look.keyWord, c->label, c->n);
        drawText(s, look.keyWord, iround(x + w - 5.5f * u - tw), centreText(iround(y), look.capH, look.wordAsc, px(11)), c->label, c->n, ink(C_TEXT, 255), 0, 0);
        break;
    }
    case IC_UP: tri(s, cx, cy - 3.6f * u, cx + 4.2f * u, cy + 2.6f * u, cx - 4.2f * u, cy + 2.6f * u, C_TEXT, 240); break;
    case IC_DOWN: tri(s, cx, cy + 3.6f * u, cx - 4.2f * u, cy - 2.6f * u, cx + 4.2f * u, cy - 2.6f * u, C_TEXT, 240); break;
    case IC_LEFT: tri(s, cx - 3.6f * u, cy, cx + 2.6f * u, cy - 4.2f * u, cx + 2.6f * u, cy + 4.2f * u, C_TEXT, 240); break;
    case IC_RIGHT: tri(s, cx + 3.6f * u, cy, cx - 2.6f * u, cy + 4.2f * u, cx - 2.6f * u, cy - 4.2f * u, C_TEXT, 240); break;
    case IC_TILDE: capsule(s, cx - 1.6f * u, cy - 3.6f * u, cx + 1.4f * u, cy + 0.4f * u, 1.15f * u, C_TEXT, 255); break; // the backtick: `
    default: {
        HANDLE f = c->n == 1 ? look.key : look.keyWord;
        int tw = textWidth(f, c->label, c->n);
        drawText(s, f, iround(cx - tw / 2.0f), centreText(iround(y), look.capH, c->n == 1 ? look.keyAsc : look.wordAsc, c->n == 1 ? px(12) : px(11)), c->label, c->n, ink(C_TEXT, 255), 0, 0);
    }
    }
}

// The controller button behind a row: A/B/X/Y as coloured circles, the rest as dark
// pills or small icons (sticks, D-pad, View, Menu).
static int badgeWidth(int pad) {
    if (pad == PAD_NONE) return 0;
    if (pad == B_LB || pad == B_RB || pad == B_LT || pad == B_RT || pad == B_LS || pad == B_RS) {
        WCHAR w[4]; int n = widen(btnName[pad], w, 4);
        int bw = textWidth(look.btnSmall, w, n) + px(12);
        return bw > px(28) ? bw : px(28);
    }
    return look.badge;
}
static void darkDisc(Surf *s, float l, float t, float r, float b) {
    float rad = (b - t) / 2, u = look.s;
    box(s, l, t + 1.2f * u, r, b + 1.2f * u, rad, BOX_FILL, 0, ink(0x000000, 140));
    box(s, l, t, r, b, rad, BOX_FILL, 0, grad(0x3C3C43, 255, 0x2A2A2F, 255, 0));
    box(s, l, t, r, b, rad, BOX_LINE, 1.0f * u, grad(0xFFFFFF, 52, 0xFFFFFF, 18, 0));
}
static void drawBadge(Surf *s, int pad, float right, float cy) {
    float u = look.s, d = (float)look.badge, l = right - badgeWidth(pad), t = cy - d / 2, b = cy + d / 2;
    WCHAR w[4]; int n = pad >= 0 && pad < NBTN ? widen(btnName[pad], w, 4) : 0;
    if (pad >= B_A && pad <= B_Y) {
        DWORD c = faceColor[pad];
        box(s, l, t + 1.2f * u, right, b + 1.2f * u, d / 2, BOX_FILL, 0, ink(0x000000, 150));
        box(s, l, t, right, b, d / 2, BOX_FILL, 0, grad(mix(c, 0xFFFFFF, 0.22f), 255, mix(c, 0x000000, 0.12f), 255, 0));
        box(s, l, t, right, b, d / 2, BOX_LINE, 1.0f * u, grad(0xFFFFFF, 70, 0xFFFFFF, 0, 0));
        int tw = textWidth(look.btn, w, n);
        drawText(s, look.btn, iround(l + d / 2 - tw / 2.0f), centreText(iround(t), look.badge, look.btnAsc, px(11)), w, n, ink(0x111114, 235), 0, 0);
        return;
    }
    if (pad == B_LB || pad == B_RB || pad == B_LT || pad == B_RT || pad == B_LS || pad == B_RS) {
        darkDisc(s, l, t, right, b);
        int tw = textWidth(look.btnSmall, w, n);
        drawText(s, look.btnSmall, iround((l + right) / 2 - tw / 2.0f), centreText(iround(t), look.badge, look.smallAsc, px(9.5f)), w, n, ink(C_TEXT, 255), 0, 0);
        return;
    }
    float cx = l + d / 2;
    if (pad >= B_DUP && pad <= B_DRIGHT) { // the D-pad, with the pressed arm lit
        float a = 2.5f * u, e = 7.2f * u;
        DWORD base = 0x45454D, lit = C_TEXT;
        darkDisc(s, l, t, right, b);
        box(s, cx - a, cy - e, cx + a, cy + e, 1.0f * u, BOX_FILL, 0, ink(base, 255));
        box(s, cx - e, cy - a, cx + e, cy + a, 1.0f * u, BOX_FILL, 0, ink(base, 255));
        if (pad == B_DUP) box(s, cx - a, cy - e, cx + a, cy - a + 0.6f * u, 1.0f * u, BOX_FILL, 0, ink(lit, 255));
        if (pad == B_DDOWN) box(s, cx - a, cy + a - 0.6f * u, cx + a, cy + e, 1.0f * u, BOX_FILL, 0, ink(lit, 255));
        if (pad == B_DLEFT) box(s, cx - e, cy - a, cx - a + 0.6f * u, cy + a, 1.0f * u, BOX_FILL, 0, ink(lit, 255));
        if (pad == B_DRIGHT) box(s, cx + a - 0.6f * u, cy - a, cx + e, cy + a, 1.0f * u, BOX_FILL, 0, ink(lit, 255));
        return;
    }
    darkDisc(s, l, t, right, b);
    if (pad == PAD_LSTICK || pad == PAD_RSTICK) { // a thumbstick: the knob, with L or R on it
        float k = 6.2f * u;
        box(s, cx - k, cy - k + 0.8f * u, cx + k, cy + k + 0.8f * u, k, BOX_FILL, 0, ink(0x000000, 120));
        box(s, cx - k, cy - k, cx + k, cy + k, k, BOX_FILL, 0, grad(0x6A6A72, 255, 0x45454C, 255, 0));
        static const WCHAR lr[2] = {'L', 'R'};
        int tw = textWidth(look.btnSmall, &lr[pad == PAD_RSTICK], 1);
        drawText(s, look.btnSmall, iround(cx - tw / 2.0f), centreText(iround(cy - k), iround(2 * k), look.smallAsc, px(9.5f)), &lr[pad == PAD_RSTICK], 1, ink(C_TEXT, 255), 0, 0);
    } else if (pad == B_BACK) { // View: two overlapping windows
        box(s, cx - 4.6f * u, cy - 3.6f * u, cx + 1.4f * u, cy + 0.9f * u, 1.0f * u, BOX_LINE, 1.2f * u, ink(C_TEXT, 230));
        box(s, cx - 1.4f * u, cy - 0.9f * u, cx + 4.6f * u, cy + 3.6f * u, 1.0f * u, BOX_FILL, 0, ink(0x2E2E33, 255));
        box(s, cx - 1.4f * u, cy - 0.9f * u, cx + 4.6f * u, cy + 3.6f * u, 1.0f * u, BOX_LINE, 1.2f * u, ink(C_TEXT, 230));
    } else if (pad == B_START) { // Menu: three lines
        for (int i = -1; i <= 1; i++) box(s, cx - 4.4f * u, cy + i * 3.0f * u - 0.65f * u, cx + 4.4f * u, cy + i * 3.0f * u + 0.65f * u, 0.65f * u, BOX_FILL, 0, ink(C_TEXT, 235));
    }
}

// Line breaks: at spaces, and between Chinese/Japanese/Korean characters (not after
// opening punctuation); a line never starts with closing punctuation or a small kana.
// Right after CJK punctuation or at a space comes first, as long as the line keeps a
// third of what fits: "クリック：回避。" rather than "...クリックしてドラッ" / "グ：...".
static int cjk(WCHAR c) { return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF); }
static int closing(WCHAR c) { return (c >= 0x3041 && c <= 0x3049 && c % 2) || c == 0x3063 || c == 0x3083 || c == 0x3085 || c == 0x3087 || (c >= 0x30A1 && c <= 0x30A9 && c % 2) || c == 0x30C3 || c == 0x30E3 || c == 0x30E5 || c == 0x30E7 || c == 0x30FC || c == 0x30FB || c == 0xFF09 || c == 0x3001 || c == 0x3002 || c == 0xFF0C || c == 0xFF1A || c == 0xFF1B || c == 0x300D || c == 0xFF01 || c == 0xFF1F || c == ')' || c == ',' || c == '.' || c == ':' || c == ';'; }
static int opening(WCHAR c) { return c == 0xFF08 || c == 0x300C || c == '('; }
static int breakAfter(WCHAR c) { return c == 0x3001 || c == 0x3002 || c == 0x30FB || c == 0xFF09 || c == 0xFF0C || c == 0xFF1A || c == 0xFF1B; } // 、。・），：；

// A row's text laid out: pieces of its UTF-16 text at offsets within the text block.
#define MAXSEG 10
typedef struct { short a, b, x, y; char note; } Seg;
static struct {
    WCHAR w[LINE_BYTES]; int n, k;   // the text, its length, where the note starts (n: none)
    Seg seg[MAXSEG]; int segs, textH; // laid out at the column's width
    int keyW, wide, col, y, h;        // key and text widths on one line, column, position
} lay[LEGEND_LINES];
static int legendCols, colX[2], colW[2], keyColW[2], textColW[2], badgeColW[2], colTop, colBottom, panelW, panelH;
static void addSeg(int i, int a, int b, int x, int y, int note) {
    if (lay[i].segs >= MAXSEG || b <= a) return;
    Seg *g = &lay[i].seg[lay[i].segs++];
    g->a = (short)a; g->b = (short)b; g->x = (short)x; g->y = (short)y; g->note = (char)note;
}
static int wrapSegs(int i, HANDLE f, int a, int b, int maxW, int y, int note, int lineHeight) {
    const WCHAR *w = lay[i].w; // (w[p + 1] below is at most w[b]: the next character, or the 0 at the end)
    SelectObject(measureDC, f);
    while (a < b && lay[i].segs < MAXSEG) {
        int fit = 0, end = b, next = b, at = 0; SIZE sz;
        if (!GetTextExtentExPointW(measureDC, w + a, b - a, maxW, &fit, 0, &sz)) fit = b - a;
        if (fit < b - a) {
            for (int p = a + fit; p > a && !at && (p - a) * 3 >= fit; p--) // after punctuation or at a space
                if (w[p] == ' ' ? !closing(w[p + 1]) : breakAfter(w[p - 1]) && !closing(w[p])) at = p;
            for (int p = a + fit; p > a && !at; p--) // else the last place this line may break
                if (w[p] == ' ' ? !closing(w[p + 1]) : (cjk(w[p - 1]) || cjk(w[p])) && !closing(w[p]) && !opening(w[p - 1])) at = p;
            end = next = at ? at : a + (fit > 0 ? fit : 1);
            if (at && w[at] == ' ') next = at + 1;
            while (end > a && w[end - 1] == ' ') end--;
        }
        addSeg(i, a, end, 0, y, note);
        y += lineHeight; a = next;
        while (a < b && w[a] == ' ') a++;
    }
    return y;
}
// The row's text at most maxW wide: on one line if it fits; otherwise the action
// (wrapped if it must) and its note on a line of its own.
static void layoutText(int i, int maxW) {
    int k = lay[i].k, n = lay[i].n, mainEnd = k;
    while (mainEnd > 0 && lay[i].w[mainEnd - 1] == ' ') mainEnd--;
    lay[i].segs = 0;
    int mw = textWidth(look.text, lay[i].w, mainEnd), nw = k < n ? textWidth(look.note, lay[i].w + k, n - k) : 0, gap = k < n ? px(5) : 0;
    if (mw + gap + nw <= maxW) {
        addSeg(i, 0, mainEnd, 0, 0, 0);
        if (k < n) addSeg(i, k, n, mw + gap, look.textAsc - look.noteAsc, 1);
        lay[i].textH = look.textH;
        return;
    }
    int y = wrapSegs(i, look.text, 0, mainEnd, maxW, 0, 0, look.textH);
    if (k < n) y = wrapSegs(i, look.note, k, n, maxW, y + px(1), 1, look.noteH);
    lay[i].textH = y;
}
static int textOneLine(int i) {
    int k = lay[i].k, n = lay[i].n, mainEnd = k;
    while (mainEnd > 0 && lay[i].w[mainEnd - 1] == ' ') mainEnd--;
    return textWidth(look.text, lay[i].w, mainEnd) + (k < n ? px(5) + textWidth(look.note, lay[i].w + k, n - k) : 0);
}
static int rowHeight(int i, int keyW) {
    Cap caps[16]; int first[9], width[9], n = rowCaps(&rows[i], caps, 16);
    int lines = capLines(caps, n, keyW, first, width);
    int h = lines * look.capH + (lines - 1) * look.capGap, t = (look.capH - look.textH) / 2 + lay[i].textH;
    if (t > h) h = t;
    if (rows[i].pad != PAD_NONE && look.badge > h) h = look.badge;
    return h;
}
static int stackHeight(int a, int b) {
    int h = 0;
    for (int i = a; i < b; i++) h += lay[i].h + (i > a ? look.rowGap + (rows[i].cat != rows[i - 1].cat ? look.groupGap : 0) : 0);
    return h;
}
// Lays the list out at the current look: one column, or two when it's long, and the
// panel's size.
static void layoutLegend(int gameH) {
    int keyAll = 0, textAll = 0;
    look.keyMax = 5 * look.capH + 4 * look.capGap;
    for (int i = 0; i < rowCount; i++) { // the move keys (W A S D) always fit on one line
        if (rows[i].pad != PAD_LSTICK) continue;
        Cap caps[16]; int n = rowCaps(&rows[i], caps, 16), w = 0;
        for (int c = 0; c < n && caps[c].group == 0; c++) w += caps[c].w + (c ? look.capGap : 0);
        if (w > look.keyMax) look.keyMax = w;
    }
    for (int i = 0; i < rowCount; i++) {
        lay[i].n = widen(rows[i].text, lay[i].w, LINE_BYTES);
        lay[i].k = rows[i].note ? MultiByteToWideChar(65001, 0, rows[i].text, rows[i].note, lay[i].w, 0) : lay[i].n;
        if (lay[i].k <= 0 || lay[i].k > lay[i].n) lay[i].k = lay[i].n;
        Cap caps[16]; int n = rowCaps(&rows[i], caps, 16), w = 0, widest = 0;
        for (int c = 0; c < n; c++) { w += caps[c].w + (c ? look.capGap : 0); if (caps[c].w > widest) widest = caps[c].w; }
        lay[i].keyW = w < look.keyMax ? w : (widest > look.keyMax ? widest : look.keyMax);
        lay[i].wide = textOneLine(i);
        if (lay[i].keyW > keyAll) keyAll = lay[i].keyW;
        if (lay[i].wide > textAll) textAll = lay[i].wide;
    }
    if (textAll > look.maxText) textAll = look.maxText;
    if (keyAll > look.keyMax) keyAll = look.keyMax;
    for (int i = 0; i < rowCount; i++) { layoutText(i, textAll); lay[i].h = rowHeight(i, keyAll); lay[i].col = 0; }
    // Two columns when it's long: split where both are about as tall, at a change of
    // group if that costs at most about a row.
    int split = rowCount, one = stackHeight(0, rowCount);
    legendCols = 1;
    if (rowCount >= 12 || (rowCount >= 6 && one > gameH * 55 / 100)) {
        int best = 1 << 30, bestAt = rowCount / 2;
        for (int k = 1; k < rowCount; k++) {
            int a = stackHeight(0, k), b = stackHeight(k, rowCount), m = a > b ? a : b;
            if (m < best) { best = m; bestAt = k; }
        }
        split = bestAt;
        for (int k = 1, nearest = 1 << 30; k < rowCount; k++) {
            int a = stackHeight(0, k), b = stackHeight(k, rowCount), m = a > b ? a : b, dist = k > bestAt ? k - bestAt : bestAt - k;
            if (rows[k].cat != rows[k - 1].cat && m <= best + 3 * (look.capH + look.rowGap) && dist < nearest) { nearest = dist; split = k; }
        }
        legendCols = 2;
    }
    for (int c = 0; c < legendCols; c++) {
        int a = c ? split : 0, b = c ? rowCount : split;
        keyColW[c] = textColW[c] = badgeColW[c] = 0;
        for (int i = a; i < b; i++) {
            lay[i].col = c;
            if (lay[i].keyW > keyColW[c]) keyColW[c] = lay[i].keyW;
            if (lay[i].wide > textColW[c]) textColW[c] = lay[i].wide;
            int bw = badgeWidth(rows[i].pad);
            if (bw > badgeColW[c]) badgeColW[c] = bw;
        }
        if (textColW[c] > look.maxText) textColW[c] = look.maxText;
        colW[c] = keyColW[c] + look.gapKT + textColW[c] + (badgeColW[c] ? look.gapTB + badgeColW[c] : 0);
        for (int i = a; i < b; i++) { layoutText(i, textColW[c]); lay[i].h = rowHeight(i, keyColW[c]); }
    }
    colTop = look.margin + look.padTop + look.titleH + look.headerGap;
    colX[0] = look.margin + look.pad;
    colX[1] = colX[0] + colW[0] + look.colGap;
    int tallest = 0;
    for (int c = 0; c < legendCols; c++) {
        int a = c ? split : 0, b = c ? rowCount : split, y = colTop;
        for (int i = a; i < b; i++) {
            if (i > a) y += look.rowGap + (rows[i].cat != rows[i - 1].cat ? look.groupGap : 0);
            lay[i].y = y; y += lay[i].h;
        }
        if (y - colTop > tallest) tallest = y - colTop;
    }
    colBottom = colTop + tallest;
    WCHAR title[64]; int tn = widen(T("Keyboard controls"), title, 64);
    panelW = 2 * look.pad + colW[0] + (legendCols > 1 ? look.colGap + colW[1] : 0);
    int tw = 2 * look.pad + textWidth(look.title, title, tn);
    if (tw > panelW) panelW = tw;
    panelH = look.padTop + look.titleH + look.headerGap + tallest + look.pad;
}

static Surf legendSurf;
static float legendScale; static int legendDirty = 1; // legendScale: the scale asked for (a small window may have made it smaller)
static void drawRow(Surf *s, int i) {
    const LegendRow *r = &rows[i]; int c = lay[i].col, x0 = colX[c], y0 = lay[i].y;
    Cap caps[16]; int first[9], width[9], n = rowCaps(r, caps, 16);
    int lines = capLines(caps, n, keyColW[c], first, width);
    for (int l = 0; l < lines; l++) { // keycaps, right-aligned to the text
        float x = (float)(x0 + keyColW[c] - width[l]), y = (float)(y0 + l * (look.capH + look.capGap));
        for (int j = first[l]; j < first[l + 1]; j++) { drawCap(s, x, y, &caps[j], r->cat); x += caps[j].w + look.capGap; }
    }
    int tx = x0 + keyColW[c] + look.gapKT, ty = y0 + (look.capH - look.textH) / 2;
    for (int g = 0; g < lay[i].segs; g++) {
        const Seg *sg = &lay[i].seg[g];
        drawText(s, sg->note ? look.note : look.text, tx + sg->x, ty + sg->y, lay[i].w + sg->a, sg->b - sg->a, ink(sg->note ? C_SUB : C_TEXT, 255), 0, 0);
    }
    if (r->pad != PAD_NONE) drawBadge(s, r->pad, (float)(x0 + colW[c]), y0 + look.capH / 2.0f);
}
// Draws the key list into legendSurf at scale s (made smaller if it wouldn't fit the
// game window). Returns 0 if the picture couldn't be made.
static int renderLegend(float s, int gameW, int gameH) {
    if (!measureDC) return 0;
    legendScale = s;
    for (int tries = 0; tries < 4; tries++) {
        lookFor(s);
        layoutLegend(gameH);
        int w = panelW + 2 * look.margin, h = panelH + 2 * look.margin;
        float fit = 1, fw = (gameW - px(24)) / (float)w, fh = (gameH - px(24)) / (float)h;
        if (gameW > 0 && fw < fit) fit = fw;
        if (gameH > 0 && fh < fit) fit = fh;
        if (fit >= 1 || s <= 0.7f) break;
        s = (float)ifloor(s * fit * 16) / 16;
        if (s < 0.7f) s = 0.7f;
    }
    int w = evenUp(panelW + 2 * look.margin), h = evenUp(panelH + 2 * look.margin);
    if (!surfMake(&legendSurf, w, h)) return 0;
    if (maskSurf.w < w || maskSurf.h < h) {
        int mw = maskSurf.w > w ? maskSurf.w : w, mh = maskSurf.h > h ? maskSurf.h : h;
        surfFree(&maskSurf);
        if (!surfMake(&maskSurf, mw, mh)) return 0;
    }
    surfClear(&legendSurf);
    Surf *S = &legendSurf;
    float l = (float)look.margin, t = (float)look.margin, r = l + panelW, b = t + panelH, rad = (float)look.radius, u = look.s;
    box(S, l, t + 5 * u, r, b + 5 * u, rad, BOX_GLOW, 18 * u, ink(0x000000, 120)); // soft shadow
    box(S, l, t, r, b, rad, BOX_FILL, 0, grad(0x1C1C1F, 236, 0x0F0F11, 240, 0));     // dark glass
    box(S, l, t, r, b, rad, BOX_LINE, 1.0f * u, grad(0xFFFFFF, 46, 0xFFFFFF, 16, 0)); // hairline, lit from above
    WCHAR title[64]; int tn = widen(T("Keyboard controls"), title, 64);
    int titleX = look.margin + look.pad, titleY = look.margin + look.padTop, tw = textWidth(look.title, title, tn);
    drawText(S, look.title, titleX, titleY, title, tn, grad(C_AMBER, 255, C_PINK, 255, 1), (float)titleX, (float)(titleX + tw));
    if (legendCols > 1) { // between the columns
        float x = colX[1] - look.colGap / 2.0f;
        box(S, x - 0.5f * u, (float)colTop + 2 * u, x + 0.5f * u, (float)colBottom - 2 * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 18));
    }
    // Under Wine a short sleep between rows lets the game's threads have GDI; on Windows
    // Sleep(1) can last a whole 15.6 ms timer tick, so it only yields there.
    for (int i = 0; i < rowCount; i++) { drawRow(S, i); Sleep(underWine ? 1 : 0); }
    { char l[96] = "Key list drawn: "; appendNum(l, sizeof(l), (U64)w); append(l, sizeof(l), " x "); appendNum(l, sizeof(l), (U64)h);
      append(l, sizeof(l), ", scale "); appendNum(l, sizeof(l), (U64)iround(look.s * 100)); append(l, sizeof(l), "%"); logline(l); }
    return 1;
}

// How big the overlays are drawn: the game window's DPI, or its height compared with
// 1080p if that's more (a 4K game at 100% gets the same proportions as 1080p), in
// steps of 1/8.
static HANDLE gameWnd;
static float overlayScale(void) {
    static UINT (*dpiForWindow)(HANDLE); static int looked;
    if (!looked) { looked = 1; HANDLE u = GetModuleHandleA("user32.dll"); if (u) dpiForWindow = GetProcAddress(u, "GetDpiForWindow"); }
    UINT dpi = dpiForWindow && gameWnd ? dpiForWindow(gameWnd) : 0;
    if (!dpi) { HANDLE dc = GetDC(0); if (dc) { dpi = (UINT)GetDeviceCaps(dc, 90 /*LOGPIXELSY*/); ReleaseDC(0, dc); } }
    float s = (dpi ? dpi : 96) / 96.0f, byHeight = gameHeight / 1080.0f;
    if (byHeight > s) s = byHeight;
    if (s < 1) s = 1;
    if (s > 3) s = 3;
    return (float)iround(s * 8) / 8;
}

// The game's main window: the largest visible window of this process that
// isn't one of the overlays.
static HANDLE typeWnd, frameWnd[4];
static LONG gameArea;
static BOOL findGameWindow(HANDLE w, LPARAM unused) {
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || w == legendWnd || w == typeWnd || !IsWindowVisible(w)) return 1;
    for (int i = 0; i < 4; i++) if (w == frameWnd[i]) return 1;
    RECT r; if (!GetClientRect(w, &r)) return 1; // just closed
    LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area > gameArea) { gameArea = area; gameWnd = w; }
    return 1;
}

// The overlays only show while the game is in front and its window isn't minimized:
// they are topmost windows, which would otherwise float over other apps (on a Mac,
// over every app, even with the game in the background). Any window of the game's
// own counts as in front, like the input side's focused(); the answer for a window
// is kept until another one comes to the front. RequireFocus=0: always.
static int gameInFront(HANDLE fg) {
    static HANDLE lastFg; static int lastOurs;
    if (!cfg.requireFocus) return gameWnd != 0;
    if (!gameWnd || !fg || IsIconic(gameWnd)) return 0;
    if (fg == gameWnd) return 1;
    if (fg != lastFg) { DWORD pid = 0; GetWindowThreadProcessId(fg, &pid); lastFg = fg; lastOurs = pid == GetCurrentProcessId(); }
    return lastOurs;
}

// Fading the key list in and out: a few steps, drawn into a copy of the picture
// (changing a window's own alpha is slow under CrossOver), then the timer stops. (The
// plain panel just shows and hides.)
static int legendFancy, legendAlpha, legendFade, legendX, legendY, legendGameW, legendGameH;
static Surf legendFaded;
#define LEGEND_FADE_TIMER 3
static void blitCopy(Surf *d, const Surf *s, int alpha);
// Shows the key list's picture at an overall alpha.
static int legendPush(int x, int y, int alpha) {
    if (alpha >= 255) return pushLayer(legendWnd, &legendSurf, x, y, 255);
    if (!surfMake(&legendFaded, legendSurf.w, legendSurf.h)) return pushLayer(legendWnd, &legendSurf, x, y, alpha);
    blitCopy(&legendFaded, &legendSurf, alpha);
    return pushLayer(legendWnd, &legendFaded, x, y, 255);
}
static void legendFadeTo(int dir) {
    if (legendFade == dir) return;
    legendFade = dir;
    SetTimer(legendWnd, LEGEND_FADE_TIMER, 15, 0); // 15, not 16: Windows rounds timers up to its 15.6 ms tick
}
static void legendFadeStep(void) {
    legendAlpha += legendFade * 40;
    if (legendAlpha >= 255 || legendAlpha <= 0 || !legendFade) {
        legendAlpha = legendAlpha >= 255 ? 255 : 0;
        KillTimer(legendWnd, LEGEND_FADE_TIMER);
        legendFade = 0;
    }
    if (!legendAlpha && !legendFade) { ShowWindow(legendWnd, 0 /*SW_HIDE*/); legendShown = 0; surfFree(&legendFaded); return; }
    legendPush(legendX, legendY, legendAlpha);
    if (legendAlpha >= 255) surfFree(&legendFaded);
}
// The game went to the background: the key list goes at once. It comes back with the
// game only while its LegendSeconds after startup last, not if F9 showed it.
static void legendAway(void) {
    legendVisible = 0;
    if (!legendShown) return;
    if (legendFade) { KillTimer(legendWnd, LEGEND_FADE_TIMER); legendFade = 0; }
    ShowWindow(legendWnd, 0 /*SW_HIDE*/);
    legendShown = 0; legendAlpha = 0;
}
static void legendToPlain(void) {
    logline("Key list: drawing the plain panel instead");
    legendFancy = 0; legendFade = 0; KillTimer(legendWnd, LEGEND_FADE_TIMER);
    SetLayeredWindowAttributes(legendWnd, 0, 215, 2 /*LWA_ALPHA*/);
    if (legendShown) { ShowWindow(legendWnd, 0); legendShown = 0; }
}

static U64 legendStarted; // when the overlay thread started
static void warmUp(void);
static int typingOnScreen(void);
static int legendWants;
static void placeLegend(void) {
    U64 now = GetTickCount64();
    int want = cfg.legend && (legendVisible || now < legendUntil);
    gameWnd = 0; gameArea = 0;
    EnumWindows(findGameWindow, 0);
    RECT r;
    if (gameWnd && gameArea >= 200 * 200 && GetClientRect(gameWnd, &r)) {
        POINT o = {0, 0}; ClientToScreen(gameWnd, &o);
        gameLeft = o.x; gameTop = o.y; gameWidth = r.right - r.left; gameHeight = r.bottom - r.top;
    } else { gameWnd = 0; gameWidth = 0; }
    if (!want && gameWnd && now - legendStarted > 3000) warmUp();
    if (want && !gameWnd) want = 0;
    // If something (e.g. Cmd+backtick) brings the key list to the front, hide it so
    // the game gets focus and the mouse back.
    if (legendShown && GetForegroundWindow() == legendWnd) { legendUntil = 0; logline("Key list was brought to the front: hidden"); }
    if (!gameInFront(GetForegroundWindow())) { legendAway(); legendWants = 0; return; }
    legendWants = want; // "wasdmod off" makes way for it
    if (typingOnScreen()) want = 0; // the typing banner (or a toast) has the top of the screen
    if (legendFancy) {
        if (!want) {
            if (legendShown && legendAlpha <= 0) { ShowWindow(legendWnd, 0); legendShown = 0; legendFade = 0; KillTimer(legendWnd, LEGEND_FADE_TIMER); }
            else if (legendShown) legendFadeTo(-1);
            return;
        }
        float s = overlayScale();
        if (legendDirty || s != legendScale || gameWidth != legendGameW || gameHeight != legendGameH) {
            if (!renderLegend(s, gameWidth, gameHeight)) { legendToPlain(); return; }
            legendDirty = 0; legendGameW = gameWidth; legendGameH = gameHeight;
            if (legendShown) legendPush(legendX, legendY, legendAlpha);
        }
        POINT p = {px(16) - look.margin, px(16) - look.margin}; ClientToScreen(gameWnd, &p);
        p.x = evenDown(p.x); p.y = evenDown(p.y);
        if (!legendShown) {
            legendX = p.x; legendY = p.y; legendAlpha = 0;
            if (!legendPush(p.x, p.y, 0)) { legendToPlain(); return; }
            SetWindowPos(legendWnd, (HANDLE)-1 /*HWND_TOPMOST*/, 0, 0, 0, 0, 0x1 | 0x2 | 0x10 | 0x40 /*NOSIZE|NOMOVE|NOACTIVATE|SHOWWINDOW*/);
            legendShown = 1;
            legendFadeTo(1);
        } else {
            if (legendFade < 0) legendFadeTo(1);
            legendX = p.x; legendY = p.y;
            SetWindowPos(legendWnd, (HANDLE)-1, p.x, p.y, 0, 0, 0x1 | 0x10 /*NOSIZE|NOACTIVATE*/);
        }
        return;
    }
    if (!want) { if (legendShown) { ShowWindow(legendWnd, 0 /*SW_HIDE*/); legendShown = 0; } return; }
    POINT p = {16, 16}; ClientToScreen(gameWnd, &p);
    SetWindowPos(legendWnd, (HANDLE)-1 /*HWND_TOPMOST*/, evenDown(p.x), evenDown(p.y), legendW, legendH, 0x10 | 0x40 /*SWP_NOACTIVATE|SWP_SHOWWINDOW*/);
    if (!legendShown) { InvalidateRect(legendWnd, 0, 1); legendShown = 1; }
}

static LRESULT legendProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT (the plain panel; the new one isn't painted this way)
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        if (!legendFancy) {
            RECT r = {0, 0, legendW, legendH}; FillRect(dc, &r, legendBrush);
            SelectObject(dc, legendFont); SetBkMode(dc, 1 /*TRANSPARENT*/);
            SetTextColor(dc, RGB(255, 214, 102));
            textOut(dc, 10, 8, T("Keyboard controls"));
            SetTextColor(dc, RGB(240, 240, 240));
            for (int i = 0; i < legendCount; i++) textOut(dc, 10, 8 + (i + 1) * lineH + 4, legendText[i]);
        }
        EndPaint(w, &ps);
        return 0;
    }
    if (msg == 0x113) { // WM_TIMER
        if (wp == LEGEND_FADE_TIMER) { legendFadeStep(); return 0; }
        reloadIfChanged(); placeLegend(); return 0;
    }
    if (msg == 0x84) return -1;                     // WM_NCHITTEST: HTTRANSPARENT, clicks go to the game
    return DefWindowProcA(w, msg, wp, lp);
}

// A hidden 1x1 window whose 30 ms timer (250 ms while the game is in the background)
// keeps the cursor in the game window, shows the typing banner and does the cursor-key nudge.
static HANDLE tickWnd;

// ---------------------------------------------------------------- banners: typing mode, mod off
// While typing mode is on (every key goes to the game until Esc), it's hard to miss:
// a banner drops in at the top centre ("Typing", a blinking caret, and a keycap that
// shows the type key, presses and flips into Esc: "Esc to play"), and an amber line
// traces itself around the game window and stays as a thin frame. Holding a movement
// key while typing (nobody holds W in a chat message) shakes the banner and blinks the
// frame. When Esc ends it, both fade away and "Back to playing" shows for a moment.
// While the toggle key has the mod switched off, the same window shows a quieter
// banner ("wasdmod off", the toggle key "to turn it on"; violet, no frame), and
// "wasdmod on" when it's switched back on. A layout saved while the game runs gets a
// third one ("Key layout loaded", the layout's name; cyan, no key, no frame) for 4 s,
// which waits for the other two and for the key list. One banner at a time.
// The windows are placed once when a banner starts and only move if the game window
// does (a window moving over the game every tick makes CrossOver stutter): the
// animations are drawn inside them, from one 15 ms timer that runs only while
// something moves. Idle, nothing is redrawn but the typing caret, twice a second.
// Without per-pixel alpha: plain banners and a plain frame, without animation.
static HANDLE typeFont, typeBrush, typeEdge, offEdge, layEdge, frameBrush; // the plain banner and frame
static int typeW, typeH, typeShown;
static int typeFancy, frameFancy, typeDirty = 1;
static Surf typeSurf, tyBase, tyWords, tyToast, ofBase, ofWords, ofToast, lyBase, lyWords, tyKey, frameSurf[4], frameFull[4];
static int frameX[4], frameY[4], frameW[4], frameH[4], frameShown;
static LONG typedL, typedT, typedW, typedH; // the game window they were placed for
static int typeX, typeY;                    // where the banner window is
#define FRAME_ALPHA 240
#define TYPE_ANIM_TIMER 7
#define TYPE_CARET_TIMER 8
#ifndef ANIM_SLOW
#define ANIM_SLOW 1 // test builds slow the animations down to capture them
#endif
#define TY_ENTER_MS 900.0f  // drop-in and frame trace 450 ms; keycap pressed at 200, flipped 430-730; words by 900
#define OFF_ENTER_MS 500.0f // "wasdmod off": the same drop-in; the words fade in by 450
#define TY_SHAKE_MS 600.0f  // banner shake 420 ms, frame blinks twice
#define TY_EXIT_MS 1550.0f  // banner and frame fade by 200 ms, then the toast
#define CARET_BLINK_FOR_MS 0 // 0: the caret blinks for as long as typing mode is on
#define C_VIOLET 0xA68CF0   // the mod keys' colour, for "wasdmod off"
#define C_VIOLET2 0x7C63D6
#define C_CYAN 0x5BB5E0     // the movement keys' colour (the editor's too), for "Key layout loaded"
#define C_CYAN2 0x2F8FBF
#define LAYOUT_SHOW_MS 4000 // "Key layout loaded" stays this long (from its drop-in), then leaves
#define LAYOUT_EXIT_MS 200.0f // it rises and fades like the others, without a toast
enum { TY_OFF, TY_ENTER, TY_ON, TY_EXIT };
enum { BN_TYPING, BN_OFF, BN_LAYOUT }; // what the banner shows
// A layout loaded while the game runs: waiting to be shown (the game in the background,
// another banner or the key list up), or shown until layoutUntil (GetTickCount64).
static int layoutPending; static U64 layoutUntil;
static int tyState, bnKind, tyTimer, tyShaking, caretLit = 1, caretTimer;
static long long tyAt, tyShakeAt, caretSince;
static float frameTraceDrawn = -1; static int frameAlphaShown = -1;
static long long animCost, animWorst; static int animFrames; // drawing time of the animation playing, for the log
static void animLog(void) {
    if (animFrames && perfFreq) { // what drawing the animation cost (on this thread, not the game's)
        char l[160] = "Banner animation: "; appendNum(l, sizeof(l), animFrames); append(l, sizeof(l), " frames, average ");
        appendNum(l, sizeof(l), (U64)(animCost * 1000000 / perfFreq / animFrames)); append(l, sizeof(l), " us, worst ");
        appendNum(l, sizeof(l), (U64)(animWorst * 1000000 / perfFreq)); append(l, sizeof(l), " us"); logline(l);
    }
    animCost = animWorst = 0; animFrames = 0;
}

// A banner's words: its label, the line around the keycap (from a "{key} to play"
// template, so each language puts the key where it belongs), the key, and the toast.
typedef struct { WCHAR label[48], pre[64], post[64], toast[64]; int labelN, preN, postN, toastN, spacePre, spacePost, keyless; Cap key; } BannerText;
static BannerText tyText, ofText, lyText; // typing mode (its key: Esc), mod off (its key: the toggle key), layout loaded (no key)
static Cap tyTypeKey;             // the type key, shown before it flips into Esc
static void wcopy(WCHAR *out, int cap, int *n, const WCHAR *from, int count) {
    while (count > 0 && *from == ' ') from++, count--;
    while (count > 0 && from[count - 1] == ' ') count--;
    if (count > cap - 1) count = cap - 1;
    for (int i = 0; i < count; i++) out[i] = from[i];
    out[count] = 0; *n = count;
}
static void bannerText(BannerText *b, const char *label, const char *line, const char *toast, const char *keyName) {
    WCHAR w[LINE_BYTES]; int n = widen(label, w, LINE_BYTES), at = -1;
    wcopy(b->label, 48, &b->labelN, w, n);
    n = widen(line, w, LINE_BYTES);
    for (int i = 0; i + 5 <= n && at < 0; i++) if (w[i] == '{' && w[i + 1] == 'k' && w[i + 2] == 'e' && w[i + 3] == 'y' && w[i + 4] == '}') at = i;
    if (at < 0) { b->preN = 0; b->spacePre = 0; b->spacePost = 1; wcopy(b->post, 64, &b->postN, w, n); }
    else {
        wcopy(b->pre, 64, &b->preN, w, at); wcopy(b->post, 64, &b->postN, w + at + 5, n - at - 5);
        b->spacePre = at > 0 && w[at - 1] == ' '; b->spacePost = at + 5 < n && w[at + 5] == ' ';
    }
    n = widen(toast, w, LINE_BYTES);
    wcopy(b->toast, 64, &b->toastN, w, n);
    memset(&b->key, 0, sizeof(b->key));
    b->keyless = !keyName;
    if (keyName) b->key.icon = capLabel(keyName, b->key.label, &b->key.n);
}
// The layout in use, as the banner names it: the editor's names for the two built-in
// files, else the file's name without .txt ("wasdmod-100726").
static void layoutName(char *out, unsigned cap) {
    const char *f = ini + len(ini);
    while (f > ini && f[-1] != '\\' && f[-1] != '/') f--;
    out[0] = 0;
    if (named(f, "default.txt")) { append(out, cap, T("Official layout")); return; }
    if (named(f, "author.txt")) { append(out, cap, T("Recommended")); return; }
    append(out, cap, f);
    unsigned n = len(out);
    if (n > 4 && out[n - 4] == '.') out[n -= 4] = 0;
    if (n > 40) { out[37] = 0; append(out, cap, "..."); } // a very long name: its start
}
static void typingParts(void) {
    char first[32] = {0}, toggle[32]; // the first type key ("T") and toggle key ("Backtick")
    for (int i = 0; typeText[i] && typeText[i] != ' ' && i < 31; i++) first[i] = typeText[i];
    nthKey(toggleKey.text, 0, toggle, sizeof(toggle));
    bannerText(&tyText, T("Typing"), T("{key} to play"), T("Back to playing"), "Escape");
    bannerText(&ofText, T("wasdmod off"), T("{key} to turn it on"), T("wasdmod on"), toggle[0] ? toggle : "Backtick");
    char name[64]; layoutName(name, sizeof(name));
    bannerText(&lyText, T("Key layout loaded"), name, "", 0);
    memset(&tyTypeKey, 0, sizeof(tyTypeKey));
    tyTypeKey.icon = capLabel(first[0] ? first : "T", tyTypeKey.label, &tyTypeKey.n);
}

// Sizes and places within the banner window: each banner's pill, centred.
typedef struct { int pillW, pillL, dotCX, labelX, labelW, caretX, divX, preX, preW, keyX, keyW, postX, toastW; } PillLay;
static struct {
    float s; int lang;
    HANDLE label, hint, key, toast;
    int labelAsc, hintAsc, keyAsc, toastAsc;
    int winW, winH, mx, mt, mb, H, keyH, keyTW, toastH, caretW, caretTop, caretBottom, labelTop, hintTop;
    PillLay typ, off, lay;
} ty;
static int tpx(float v) { return iround(v * ty.s); }
static int keyCapW(const Cap *c, int keyH) {
    if (c->icon >= IC_MOUSE1 && c->icon <= IC_MOUSE5) return keyH + (c->icon >= IC_MOUSE4 ? tpx(8) : 0);
    if (c->icon != IC_NONE) return keyH;
    int w = textWidth(ty.key, c->label, c->n) + tpx(16);
    return w > keyH ? w : keyH;
}
// A pill's parts, from its left edge (moved into the window by placePill).
static void layoutPill(PillLay *p, const BannerText *b, int caret) {
    int padL = tpx(22), dot = tpx(10), dotGap = tpx(12), divGap = tpx(18), padR = tpx(22);
    int gapPre = b->preN ? (b->spacePre ? tpx(8) : tpx(3)) : 0, gapPost = b->postN && !b->keyless ? (b->spacePost ? tpx(8) : tpx(3)) : 0;
    int caretSpace = caret ? tpx(2) + ty.caretW : 0, postW = textWidth(ty.hint, b->post, b->postN);
    p->labelW = textWidth(ty.label, b->label, b->labelN);
    p->keyW = b->keyless ? 0 : keyCapW(&b->key, ty.keyH);
    p->preW = textWidth(ty.hint, b->pre, b->preN);
    p->dotCX = padL + dot / 2;
    p->labelX = padL + dot + dotGap;
    p->caretX = p->labelX + p->labelW + tpx(2);
    p->divX = p->labelX + p->labelW + caretSpace + divGap;
    p->preX = p->divX + tpx(1) + divGap;
    p->keyX = p->preX + p->preW + gapPre;
    p->postX = p->keyX + p->keyW + gapPost;
    p->pillW = p->postX + postW + padR;
    p->toastW = tpx(16) + tpx(24) + tpx(11) + textWidth(ty.toast, b->toast, b->toastN) + tpx(20);
}
static void placePill(PillLay *p) {
    p->pillL = (ty.winW - p->pillW) / 2;
    p->dotCX += p->pillL; p->labelX += p->pillL; p->caretX += p->pillL; p->divX += p->pillL;
    p->preX += p->pillL; p->keyX += p->pillL; p->postX += p->pillL;
}
static void layoutTyping(float s) {
    if (!ty.label || ty.s != s || ty.lang != langIndex) {
        HANDLE *fonts[] = {&ty.label, &ty.hint, &ty.key, &ty.toast};
        unselectFonts();
        for (int i = 0; i < 4; i++) if (*fonts[i]) { DeleteObject(*fonts[i]); *fonts[i] = 0; }
        ty.s = s; ty.lang = langIndex;
        int h;
        ty.label = uiFont(tpx(22), 700, 0); ty.hint = uiFont(tpx(16), 500, 0); ty.key = uiFont(tpx(14), 700, 1); ty.toast = uiFont(tpx(15), 600, 0);
        spacedFont[2] = ty.key;
        fontMetrics(ty.label, &ty.labelAsc, &h); fontMetrics(ty.hint, &ty.hintAsc, &h);
        fontMetrics(ty.key, &ty.keyAsc, &h); fontMetrics(ty.toast, &ty.toastAsc, &h);
    }
    ty.H = tpx(58); ty.keyH = tpx(28); ty.toastH = tpx(44);
    ty.caretW = tpx(2) > 2 ? tpx(2) : 2;
    ty.keyTW = keyCapW(&tyTypeKey, ty.keyH);
    layoutPill(&ty.typ, &tyText, 1); layoutPill(&ty.off, &ofText, 0); layoutPill(&ty.lay, &lyText, 0);
    int widest = ty.typ.pillW;
    if (ty.typ.toastW > widest) widest = ty.typ.toastW;
    if (ty.off.pillW > widest) widest = ty.off.pillW;
    if (ty.off.toastW > widest) widest = ty.off.toastW;
    if (ty.lay.pillW > widest) widest = ty.lay.pillW;
    ty.mx = tpx(32); ty.mt = tpx(30); ty.mb = tpx(38);
    ty.winW = evenUp(widest + 2 * ty.mx); ty.winH = evenUp(ty.mt + ty.H + ty.mb);
    placePill(&ty.typ); placePill(&ty.off); placePill(&ty.lay);
    ty.labelTop = centreText(ty.mt, ty.H, ty.labelAsc, tpx(22));
    int baseline = ty.labelTop + ty.labelAsc;
    ty.caretTop = baseline - tpx(22 * 0.74f); ty.caretBottom = baseline + tpx(1);
    ty.hintTop = centreText(ty.mt, ty.H, ty.hintAsc, tpx(16));
}
// The parts that don't move against each other, drawn once: the pill with its dot,
// label and divider (in the banner's colours, c1 to c2); the words around the keycap;
// the toast.
static void renderBase(Surf *S, const PillLay *p, const BannerText *b, DWORD c1, DWORD c2, DWORD dotTop, int glowA, int edgeA) {
    surfClear(S);
    float u = ty.s, l = (float)p->pillL, t = (float)ty.mt, r = l + p->pillW, bt = t + ty.H, rad = ty.H / 2.0f, cy = t + ty.H / 2.0f;
    box(S, l, t + 7 * u, r, bt + 7 * u, rad, BOX_GLOW, 20 * u, ink(0x000000, 150));     // shadow
    box(S, l, t, r, bt, rad, BOX_GLOW, 14 * u, grad(c1, glowA, c2, glowA, 1));           // glow
    box(S, l, t, r, bt, rad, BOX_FILL, 0, grad(0x232327, 246, 0x111113, 248, 0));         // dark glass
    box(S, l, t, r, bt, rad, BOX_LINE, 1.6f * u, grad(c1, edgeA, c2, edgeA, 1));          // edge
    box(S, l + rad * 0.6f, t + 2 * u, r - rad * 0.6f, t + 3 * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 22));
    float dr = 5.0f * u, dx = (float)p->dotCX;
    box(S, dx - dr, cy - dr, dx + dr, cy + dr, dr, BOX_GLOW, 7 * u, ink(c1, 120));
    box(S, dx - dr, cy - dr, dx + dr, cy + dr, dr, BOX_FILL, 0, grad(dotTop, 255, c2, 255, 0));
    drawText(S, ty.label, p->labelX, ty.labelTop, b->label, b->labelN, ink(C_TEXT, 255), 0, 0);
    box(S, (float)p->divX, cy - 14 * u, p->divX + 1 * u, cy + 14 * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 46));
}
static void renderWords(Surf *S, const PillLay *p, const BannerText *b) {
    surfClear(S);
    if (b->preN) drawText(S, ty.hint, p->preX, ty.hintTop, b->pre, b->preN, ink(C_TEXT, 255), 0, 0);
    if (b->postN) drawText(S, ty.hint, p->postX, ty.hintTop, b->post, b->postN, ink(C_TEXT, 255), 0, 0);
}
// A small green controller (the toasts' icon), 24 x 16 at scale 1, left edge x.
static void drawPadGlyph(Surf *S, float x, float cy, float u) {
    float t = cy - 8 * u;
    Ink green = grad(0x8BE09B, 255, 0x4FAE63, 255, 0);
    box(S, x + 1.5f * u, t + 1.0f * u, x + 22.5f * u, t + 11.5f * u, 5.0f * u, BOX_FILL, 0, green);
    box(S, x + 0.5f * u, t + 5.0f * u, x + 9.5f * u, t + 15.5f * u, 4.5f * u, BOX_FILL, 0, green);
    box(S, x + 14.5f * u, t + 5.0f * u, x + 23.5f * u, t + 15.5f * u, 4.5f * u, BOX_FILL, 0, green);
    DWORD hole = 0x16161A;
    box(S, x + 5.2f * u, t + 5.1f * u, x + 10.0f * u, t + 6.9f * u, 0.5f * u, BOX_FILL, 0, ink(hole, 255)); // D-pad
    box(S, x + 6.7f * u, t + 3.6f * u, x + 8.5f * u, t + 8.4f * u, 0.5f * u, BOX_FILL, 0, ink(hole, 255));
    float bx = x + 17.0f * u, by = t + 6.0f * u, r = 1.15f * u;
    for (int i = 0; i < 4; i++) {
        float ox = i == 0 ? 0 : i == 1 ? 2.3f * u : i == 2 ? 0 : -2.3f * u, oy = i == 0 ? -2.3f * u : i == 2 ? 2.3f * u : 0;
        box(S, bx + ox - r, by + oy - r, bx + ox + r, by + oy + r, r, BOX_FILL, 0, ink(hole, 255));
    }
}
static void renderToast(Surf *S, const BannerText *b, int toastW) {
    surfClear(S);
    float u = ty.s, h = (float)ty.toastH, w = (float)toastW, l = (ty.winW - w) / 2, t = ty.mt + (ty.H - h) / 2, r = l + w, bt = t + h, rad = h / 2;
    box(S, l, t + 6 * u, r, bt + 6 * u, rad, BOX_GLOW, 18 * u, ink(0x000000, 140));
    box(S, l, t, r, bt, rad, BOX_GLOW, 10 * u, ink(0x6CC27F, 40));
    box(S, l, t, r, bt, rad, BOX_FILL, 0, grad(0x232327, 246, 0x111113, 248, 0));
    box(S, l, t, r, bt, rad, BOX_LINE, 1.2f * u, grad(0x8BE09B, 150, 0x5CBF5C, 90, 1));
    box(S, l + rad * 0.6f, t + 2 * u, r - rad * 0.6f, t + 3 * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 20));
    drawPadGlyph(S, l + 16 * u, t + h / 2, u);
    drawText(S, ty.toast, iround(l + 16 * u + 24 * u + 11 * u), centreText(iround(t), ty.toastH, ty.toastAsc, tpx(15)), b->toast, b->toastN, ink(C_TEXT, 255), 0, 0);
}
// The keycap, unsquashed, into tyKey: w wide, pushed down by press pixels, tinted with
// the banner's colour.
static void renderTypeKey(const Cap *c, int w, int press, DWORD tint) {
    int pad = tpx(3), kh = ty.keyH, most = ty.typ.keyW;
    if (ty.keyTW > most) most = ty.keyTW;
    if (ty.off.keyW > most) most = ty.off.keyW;
    if (!surfMake(&tyKey, most + 2 * pad, kh + 2 * pad + tpx(3))) return;
    surfClear(&tyKey);
    float u = ty.s, x = (float)(tyKey.w - w) / 2, y = (float)(pad + press), kr = 6 * u, cx = x + w / 2.0f, cy = y + kh / 2.0f;
    box(&tyKey, x, pad + 1.8f * u, x + w, pad + kh + 1.8f * u, kr, BOX_FILL, 0, ink(0x000000, 170)); // the lower edge, hidden when pressed
    box(&tyKey, x, y, x + w, y + kh, kr, BOX_FILL, 0, grad(mix(0x38383D, tint, 0.22f), 255, mix(0x222226, tint, 0.14f), 255, 0));
    box(&tyKey, x, y, x + w, y + kh, kr, BOX_LINE, 1.0f * u, grad(mix(0x7A7A82, tint, 0.78f), 235, mix(0x3A3A40, tint, 0.55f), 205, 0));
    box(&tyKey, x + kr, y + 1.0f * u, x + w - kr, y + 2.0f * u, 0.5f * u, BOX_FILL, 0, ink(0xFFFFFF, 32));
    float k = 1.2f * u; // icons a little bigger than in the key list
    switch (c->icon) {
    case IC_MOUSE1: case IC_MOUSE2: case IC_MOUSE3: case IC_MOUSE4: case IC_MOUSE5: {
        float old = look.s; look.s = u * 1.15f;
        drawMouse(&tyKey, cx - (c->icon >= IC_MOUSE4 ? 3 * u : 0), cy, c->icon - IC_MOUSE1 + 1, tint);
        look.s = old;
        break;
    }
    case IC_UP: tri(&tyKey, cx, cy - 3.6f * k, cx + 4.2f * k, cy + 2.6f * k, cx - 4.2f * k, cy + 2.6f * k, C_TEXT, 240); break;
    case IC_DOWN: tri(&tyKey, cx, cy + 3.6f * k, cx - 4.2f * k, cy - 2.6f * k, cx + 4.2f * k, cy - 2.6f * k, C_TEXT, 240); break;
    case IC_LEFT: tri(&tyKey, cx - 3.6f * k, cy, cx + 2.6f * k, cy - 4.2f * k, cx + 2.6f * k, cy + 4.2f * k, C_TEXT, 240); break;
    case IC_RIGHT: tri(&tyKey, cx + 3.6f * k, cy, cx - 2.6f * k, cy + 4.2f * k, cx - 2.6f * k, cy - 4.2f * k, C_TEXT, 240); break;
    case IC_TILDE: capsule(&tyKey, cx - 1.6f * k, cy - 3.6f * k, cx + 1.4f * k, cy + 0.4f * k, 1.15f * k, C_TEXT, 255); break; // the backtick: `
    default:
        if (c->n) {
            int tw = textWidth(ty.key, c->label, c->n);
            drawText(&tyKey, ty.key, iround(cx - tw / 2.0f), centreText(iround(y), kh, ty.keyAsc, tpx(14)), c->label, c->n, ink(C_TEXT, 255), 0, 0);
        }
    }
}

// Puts picture s over d at (dx, dy), with an extra overall alpha.
static void blitA(Surf *d, const Surf *s, int dx, int dy, int alpha) {
    if (alpha <= 0 || !s->px) return;
    if (alpha > 255) alpha = 255;
    int x0 = dx < 0 ? -dx : 0, y0 = dy < 0 ? -dy : 0, x1 = s->w, y1 = s->h;
    if (dx + x1 > d->w) x1 = d->w - dx;
    if (dy + y1 > d->h) y1 = d->h - dy;
    for (int y = y0; y < y1; y++) {
        const DWORD *sp = s->px + (long)y * s->w; DWORD *dp = d->px + (long)(y + dy) * d->w + dx;
        for (int x = x0; x < x1; x++) {
            DWORD p = sp[x];
            if (!p) continue;
            int sa = (int)(p >> 24), sr = (int)(p >> 16) & 255, sg = (int)(p >> 8) & 255, sb = (int)p & 255;
            if (alpha < 255) { sa = sa * alpha / 255; sr = sr * alpha / 255; sg = sg * alpha / 255; sb = sb * alpha / 255; }
            DWORD q = dp[x]; int k = 255 - sa;
            dp[x] = (DWORD)(sa + ((int)(q >> 24) * k + 127) / 255) << 24 | (DWORD)(sr + ((int)((q >> 16) & 255) * k + 127) / 255) << 16
                  | (DWORD)(sg + ((int)((q >> 8) & 255) * k + 127) / 255) << 8 | (DWORD)(sb + ((int)(q & 255) * k + 127) / 255);
        }
    }
}
// A copy of s (the same size) at an overall alpha.
static void blitCopy(Surf *d, const Surf *s, int alpha) {
    long n = (long)s->w * s->h;
    if (!d->px || !s->px || d->w != s->w || d->h != s->h) return;
    if (alpha >= 255) { for (long i = 0; i < n; i++) d->px[i] = s->px[i]; return; }
    for (long i = 0; i < n; i++) {
        DWORD p = s->px[i];
        d->px[i] = !p ? 0 : ((p >> 24) * alpha / 255) << 24 | (((p >> 16) & 255) * alpha / 255) << 16 | (((p >> 8) & 255) * alpha / 255) << 8 | ((p & 255) * alpha / 255);
    }
}
// The same, squashed vertically by sy: source row sAnchor lands on row dAnchor of d
// (the keycap flipping over).
static void blitSquash(Surf *d, const Surf *s, int dx, float dAnchor, float sAnchor, float sy, int alpha) {
    if (sy <= 0.03f || alpha <= 0 || !s->px) return;
    int y0 = ifloor(dAnchor - sAnchor * sy) - 1, y1 = ifloor(dAnchor + (s->h - sAnchor) * sy) + 1;
    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= d->h) continue;
        float src = sAnchor + (y + 0.5f - dAnchor) / sy - 0.5f; int r0 = ifloor(src); float f = src - r0;
        for (int x = 0; x < s->w; x++) {
            int xx = x + dx;
            if (xx < 0 || xx >= d->w) continue;
            DWORD p0 = r0 >= 0 && r0 < s->h ? s->px[(long)r0 * s->w + x] : 0, p1 = r0 + 1 >= 0 && r0 + 1 < s->h ? s->px[(long)(r0 + 1) * s->w + x] : 0;
            if (!p0 && !p1) continue;
            int ch[4];
            for (int c = 0; c < 4; c++) {
                int a0 = (int)(p0 >> (c * 8)) & 255, a1 = (int)(p1 >> (c * 8)) & 255;
                ch[c] = (int)((a0 + (a1 - a0) * f) * alpha / 255.0f + 0.5f);
            }
            DWORD q = d->px[(long)y * d->w + xx]; int k = 255 - ch[3]; DWORD out = 0;
            for (int c = 0; c < 4; c++) out |= (DWORD)(ch[c] + ((int)((q >> (c * 8)) & 255) * k + 127) / 255) << (c * 8);
            d->px[(long)y * d->w + xx] = out;
        }
    }
}

// The animation's curves: what the banner, keycap, caret, frame and toast look like t
// ms into the current state (and st ms into a shake).
typedef struct { float dx, dy, press, squash, words, trace, toastDy; int alpha, esc, caret, frame, toast, toastA; } TyFrame;
static float easeOut(float p) { p = clamp01(p); float q = 1 - p; return 1 - q * q * q; }
static float easeIn(float p) { p = clamp01(p); return p * p; }
static float bump(float p) { p = clamp01(p); return smooth01(p < 0.5f ? 2 * p : 2 - 2 * p); }
static void typingFrame(int state, float t, int shaking, float st, int caretOn, TyFrame *f) {
    memset(f, 0, sizeof(*f));
    f->alpha = 255; f->esc = 1; f->squash = 1; f->words = 1; f->trace = 1; f->frame = FRAME_ALPHA; f->caret = caretOn;
    float drop = (float)tpx(26), over = (float)tpx(5);
    if (state == TY_ENTER) {
        f->caret = 1;
        // Drops in from above, 5 px past its place by 300 ms, settled at 450 ms; fades in.
        f->dy = t < 300 ? -drop + (drop + over) * easeOut(t / 300) : t < 450 ? over * (1 - smooth01((t - 300) / 150)) : 0;
        f->alpha = (int)(255 * clamp01(t / 220) * (2 - clamp01(t / 220)));
        f->trace = easeOut(t / 450);
        // The keycap: the type key, pressed at 200 ms, flipped into Esc from 430 to 730 ms.
        f->esc = t >= 580;
        float down = 2 * ty.s;
        f->press = t < 200 ? 0 : t < 260 ? down * (t - 200) / 60 : t < 430 ? down : t < 580 ? down * (1 - (t - 430) / 150) : 0;
        f->squash = t < 430 ? 1 : t < 580 ? 1 - easeIn((t - 430) / 150) : easeOut((t - 580) / 150);
        f->words = clamp01((t - 730) / 170);
    }
    if (shaking && state != TY_EXIT) { // -9, +8, -6, +4 px, then rest; the frame blinks twice
        static const float keys[6] = {0, -9, 8, -6, 4, 0};
        float seg = st / 84; int k = (int)seg;
        if (k < 5) f->dx = (keys[k] + (keys[k + 1] - keys[k]) * smooth01(seg - k)) * ty.s;
        f->frame = (int)(FRAME_ALPHA * (1 - 0.85f * bump((st < 300 ? st : st - 300) / 300)));
        f->caret = 1;
    }
    if (state == TY_EXIT) { // rises 10 px and fades (180 ms), the frame fades (200 ms), then the toast
        f->caret = 1;
        f->alpha = (int)(255 * (1 - clamp01(t / 180)));
        f->dy = -10 * ty.s * easeOut(t / 180);
        f->frame = (int)(FRAME_ALPHA * (1 - clamp01(t / 200)));
        if (t >= 200) {
            float v = t - 200;
            f->toast = 1;
            f->toastA = (int)(255 * (v < 150 ? easeOut(v / 150) : v > 1150 ? 1 - clamp01((v - 1150) / 200) : 1));
            f->toastDy = v < 150 ? -6 * ty.s * (1 - easeOut(v / 150)) : 0;
        }
    }
    if (bnKind == BN_OFF || bnKind == BN_LAYOUT) { // "wasdmod off", "Key layout loaded": the same drop-in and exit; no caret, no frame
        f->caret = 0; f->esc = 1; f->press = 0; f->squash = 1; f->trace = 1; f->frame = 0;
        if (state == TY_ENTER) f->words = clamp01((t - 250) / 200);
        if (bnKind == BN_LAYOUT && state == TY_EXIT) f->toast = 0; // just leaves
    }
}
// Draws a frame of the banner (or its toast) into typeSurf.
static void composeTyping(const TyFrame *f) {
    int off = bnKind == BN_OFF;
    surfClear(&typeSurf);
    if (bnKind == BN_LAYOUT) { // no key: the pill and the layout's name
        int ix = iround(f->dx), iy = iround(f->dy);
        blitA(&typeSurf, &lyBase, ix, iy, f->alpha);
        if (f->words > 0) blitA(&typeSurf, &lyWords, ix, iy, (int)(f->alpha * f->words));
        return;
    }
    const PillLay *p = off ? &ty.off : &ty.typ;
    if (f->toast) { blitA(&typeSurf, off ? &ofToast : &tyToast, 0, iround(f->toastDy), f->toastA); return; }
    int ix = iround(f->dx), iy = iround(f->dy);
    blitA(&typeSurf, off ? &ofBase : &tyBase, ix, iy, f->alpha);
    if (f->words > 0) blitA(&typeSurf, off ? &ofWords : &tyWords, ix, iy, (int)(f->alpha * f->words));
    const Cap *c = off ? &ofText.key : f->esc ? &tyText.key : &tyTypeKey;
    int w = off || f->esc ? p->keyW : ty.keyTW, pad = tpx(3);
    renderTypeKey(c, w, iround(f->press), off ? C_VIOLET : C_AMBER);
    float centre = ty.mt + ty.H / 2.0f + iy;
    blitSquash(&typeSurf, &tyKey, iround(p->keyX + p->keyW / 2.0f - tyKey.w / 2.0f) + ix, centre, pad + ty.keyH / 2.0f, f->squash, f->alpha);
    if (f->caret) box(&typeSurf, (float)(p->caretX + ix), (float)(ty.caretTop + iy), (float)(p->caretX + ty.caretW + ix), (float)(ty.caretBottom + iy), 0.6f * ty.s, BOX_FILL, 0, ink(C_AMBER, f->alpha));
}

// The frame: four thin strips along the game window's edges (cheap, and the game stays
// uncovered): an amber line with a dark hairline inside it (so it shows on bright
// scenes too) and a faint glow. trace (0-1) draws only part of it, growing both ways
// from the top centre and meeting at the bottom centre.
// The strips (top, bottom, left, right), T thick, for a game window at screen (L, Tp),
// W x H; relative to the window. Under Wine their screen edges are even (see evenUp),
// so a strip can reach a pixel past the window, where it stays transparent.
static void frameStrips(int L, int Tp, int W, int H, int T, int *x, int *y, int *w, int *h) {
    int x0 = evenDown(L) - L, x1 = evenUp(L + T) - L, x2 = evenDown(L + W - T) - L, x3 = evenUp(L + W) - L;
    int y0 = evenDown(Tp) - Tp, y1 = evenUp(Tp + T) - Tp, y2 = evenDown(Tp + H - T) - Tp, y3 = evenUp(Tp + H) - Tp;
    x[0] = x0; y[0] = y0; w[0] = x3 - x0; h[0] = y1 - y0; // top
    x[1] = x0; y[1] = y2; w[1] = x3 - x0; h[1] = y3 - y2; // bottom
    x[2] = x0; y[2] = y1; w[2] = x1 - x0; h[2] = y2 - y1; // left
    x[3] = x2; y[3] = y1; w[3] = x3 - x2; h[3] = y2 - y1; // right
}
static int renderFrame(float s, int L, int Tp, int W, int H, float trace) {
    int line = iround(3.5f * s), glow = iround(10 * s), T = line + 1 + glow;
    if (line < 3) line = 3;
    if (W < 8 * T || H < 8 * T) return 0;
    frameStrips(L, Tp, W, H, T, frameX, frameY, frameW, frameH);
    float half = (float)(W + H), head = 36 * s, reach = trace * (half + head);
    for (int i = 0; i < 4; i++) {
        if (!surfMake(&frameFull[i], frameW[i], frameH[i]) || !surfMake(&frameSurf[i], frameW[i], frameH[i])) return 0;
        Surf *S = &frameFull[i];
        for (int yy = 0; yy < S->h; yy++) {
            DWORD *row = S->px + (long)yy * S->w;
            for (int xx = 0; xx < S->w; xx++) {
                int fx = frameX[i] + xx, fy = frameY[i] + yy, d = fy, e = 0;
                if (fx < 0 || fy < 0 || fx >= W || fy >= H) { row[xx] = 0; continue; } // just past the window (even edges)
                if (H - 1 - fy < d) d = H - 1 - fy, e = 1;
                if (fx < d) d = fx, e = 2;
                if (W - 1 - fx < d) d = W - 1 - fx, e = 2;
                float cx = fx + 0.5f - W / 2.0f; if (cx < 0) cx = -cx;
                float path = e == 0 ? cx : e == 1 ? W / 2.0f + H + (W / 2.0f - cx) : W / 2.0f + fy + 0.5f;
                float a, lead = 1; DWORD c = C_AMBER;
                if (d < line) a = 240;
                else if (d == line) { a = 90; c = 0x000000; }
                else { float g = 1 - (d - line - 0.5f) / glow; a = g > 0 ? 70 * g * g : 0; }
                if (trace < 1) {
                    lead = clamp01((reach - path) / head);
                    float nearHead = clamp01(1 - (reach - head - path) / (60 * s)); // the moving end glows brighter
                    if (c != 0 && nearHead > 0) c = mix(C_AMBER, 0xFFF0CC, nearHead * 0.7f);
                }
                int ai = (int)(a * lead + 0.5f);
                if (ai <= 0) { row[xx] = 0; continue; }
                if (ai > 255) ai = 255;
                row[xx] = (DWORD)ai << 24 | (((c >> 16) & 255) * ai / 255) << 16 | (((c >> 8) & 255) * ai / 255) << 8 | ((c & 255) * ai / 255);
            }
        }
    }
    return 1;
}

static void typingToPlain(void) {
    logline("Banner: drawing the plain one instead");
    typeFancy = frameFancy = 0;
    SetLayeredWindowAttributes(typeWnd, 0, 230, 2 /*LWA_ALPHA*/);
    for (int i = 0; i < 4; i++) if (frameWnd[i]) SetLayeredWindowAttributes(frameWnd[i], 0, FRAME_ALPHA, 2);
}
// The banners' fixed parts at scale s (again only if the scale, the language or the
// keys changed).
static int renderTypingParts(float s) {
    if (!typeDirty && tyBase.px && ty.s == s) return 1;
    if (typeDirty) typingParts();
    layoutTyping(s);
    int mw = maskSurf.w > ty.winW ? maskSurf.w : ty.winW, mh = maskSurf.h > ty.winH ? maskSurf.h : ty.winH;
    if (mw > maskSurf.w || mh > maskSurf.h) { surfFree(&maskSurf); if (!surfMake(&maskSurf, mw, mh)) return 0; }
    Surf *all[] = {&typeSurf, &tyBase, &tyWords, &tyToast, &ofBase, &ofWords, &ofToast, &lyBase, &lyWords};
    for (int i = 0; i < 9; i++) if (!surfMake(all[i], ty.winW, ty.winH)) return 0;
    renderBase(&tyBase, &ty.typ, &tyText, C_AMBER, C_ORANGE, 0xFFD58A, 60, 255);
    renderWords(&tyWords, &ty.typ, &tyText);
    renderToast(&tyToast, &tyText, ty.typ.toastW);
    renderBase(&ofBase, &ty.off, &ofText, C_VIOLET, C_VIOLET2, 0xD9CCFF, 34, 170);
    renderWords(&ofWords, &ty.off, &ofText);
    renderToast(&ofToast, &ofText, ty.off.toastW);
    renderBase(&lyBase, &ty.lay, &lyText, C_CYAN, C_CYAN2, 0xC4E8F8, 48, 220);
    renderWords(&lyWords, &ty.lay, &lyText);
    typeDirty = 0;
    return 1;
}
// Sizes the banner for the game window (drawing its fixed parts again if needed) and
// works out where its window goes.
static int placeTyping(void) {
    float s = overlayScale();
    if (typedL != gameLeft || typedT != gameTop || typedW != gameWidth || typedH != gameHeight || ty.s != s) frameTraceDrawn = -1; // the frame, drawn again for this window
    typedL = gameLeft; typedT = gameTop; typedW = gameWidth; typedH = gameHeight;
    if (!renderTypingParts(s)) return 0;
    int top = tpx(18) > gameHeight * 4 / 100 ? tpx(18) : (int)(gameHeight * 4 / 100);
    typeX = evenDown(gameLeft + (gameWidth - ty.winW) / 2); typeY = evenDown(gameTop + top - ty.mt);
    frameAlphaShown = -1;
    return 1;
}
static void typingHide(void) {
    if (tyTimer) { KillTimer(typeWnd, TYPE_ANIM_TIMER); tyTimer = 0; animLog(); }
    if (caretTimer) { KillTimer(typeWnd, TYPE_CARET_TIMER); caretTimer = 0; }
    ShowWindow(typeWnd, 0 /*SW_HIDE*/);
    for (int i = 0; i < 4; i++) if (frameWnd[i]) ShowWindow(frameWnd[i], 0);
    tyState = TY_OFF; tyShaking = 0; frameShown = 0; typeShown = 0;
}
static void animTimer(void) { if (!tyTimer) tyTimer = SetTimer(typeWnd, TYPE_ANIM_TIMER, 15, 0) != 0; } // 15: one Windows timer tick
static void caretStart(void) {
    UINT blink = GetCaretBlinkTime();
    caretLit = 1;
    if (bnKind == BN_TYPING && blink && blink < 10000 && !caretTimer) { caretTimer = SetTimer(typeWnd, TYPE_CARET_TIMER, blink, 0) != 0; QueryPerformanceCounter(&caretSince); }
}
// The one animation step: works out the current frame from the clock, draws it into
// the windows, and stops the timer when nothing moves any more.
static void drawTyping(void) {
    if (tyState == TY_OFF) return;
    long long now; QueryPerformanceCounter(&now);
    float t = perfFreq ? (float)(now - tyAt) * 1000.0f / (float)perfFreq / ANIM_SLOW : 1e6f;
    float st = perfFreq ? (float)(now - tyShakeAt) * 1000.0f / (float)perfFreq / ANIM_SLOW : 1e6f;
    if (tyState == TY_ENTER && t >= (bnKind == BN_TYPING ? TY_ENTER_MS : OFF_ENTER_MS)) { tyState = TY_ON; caretStart(); } // in place
    if (tyShaking && st >= TY_SHAKE_MS) tyShaking = 0;
    if (tyState == TY_EXIT && t >= (bnKind == BN_LAYOUT ? LAYOUT_EXIT_MS : TY_EXIT_MS)) { typingHide(); return; }
    TyFrame f; typingFrame(tyState, t, tyShaking, st, caretLit, &f);
    long long began; QueryPerformanceCounter(&began);
    composeTyping(&f);
    if (!pushLayer(typeWnd, &typeSurf, typeX, typeY, 255)) { typingToPlain(); typingHide(); return; }
    if (frameFancy && frameShown) {
        // Fading and blinking are drawn into the strips' pictures: changing a window's own
        // alpha each frame is much slower under CrossOver.
        int retrace = f.trace != frameTraceDrawn;
        if (retrace) {
            if (renderFrame(ty.s, typedL, typedT, typedW, typedH, f.trace)) frameTraceDrawn = f.trace;
            else { frameShown = 0; for (int i = 0; i < 4; i++) if (frameWnd[i]) ShowWindow(frameWnd[i], 0); } // no frame this time (e.g. a window too big or too small for it); tried again next time
        }
        if (frameShown && (retrace || f.frame != frameAlphaShown)) {
            for (int i = 0; i < 4; i++) {
                if (!frameWnd[i]) continue;
                blitCopy(&frameSurf[i], &frameFull[i], f.frame);
                pushLayer(frameWnd[i], &frameSurf[i], typedL + frameX[i], typedT + frameY[i], 255);
            }
            frameAlphaShown = f.frame;
        }
        if (f.frame <= 0 && tyState == TY_EXIT) { for (int i = 0; i < 4; i++) if (frameWnd[i]) ShowWindow(frameWnd[i], 0); frameShown = 0; }
    }
    long long done; QueryPerformanceCounter(&done);
    int moving = tyState == TY_ENTER || tyState == TY_EXIT || tyShaking;
    if (tyTimer) { animCost += done - began; if (done - began > animWorst) animWorst = done - began; animFrames++; }
    if (!moving && tyTimer) { KillTimer(typeWnd, TYPE_ANIM_TIMER); tyTimer = 0; animLog(); }
}
// Shows the banner's windows (after its first frame is drawn).
static void typingShow(void) {
    for (int i = 0; i < 4 && frameShown; i++) if (frameWnd[i]) SetWindowPos(frameWnd[i], (HANDLE)-1, 0, 0, 0, 0, 0x1 | 0x2 | 0x10 | 0x40 /*NOSIZE|NOMOVE|NOACTIVATE|SHOWWINDOW*/);
    SetWindowPos(typeWnd, (HANDLE)-1, 0, 0, 0, 0, 0x1 | 0x2 | 0x10 | 0x40);
    typeShown = 1;
}
// A banner that was up as the game comes back to the front (or the key list goes
// away): as it was, without the entry animation (the typing caret blinks again).
static void typingSettled(int kind) {
    if (!placeTyping()) { typingToPlain(); return; }
    bnKind = kind; tyState = TY_ON; tyShaking = 0; caretLit = 1; frameTraceDrawn = -1;
    frameShown = frameFancy && kind == BN_TYPING;
    drawTyping();
    if (tyState == TY_OFF) return;
    typingShow();
    caretStart();
}
static void typingStart(int kind) {
    if (!placeTyping()) { typingToPlain(); return; }
    if (caretTimer) { KillTimer(typeWnd, TYPE_CARET_TIMER); caretTimer = 0; }
    bnKind = kind; tyState = TY_ENTER; tyShaking = 0; caretLit = 1; frameTraceDrawn = -1;
    QueryPerformanceCounter(&tyAt);
    frameShown = frameFancy && kind == BN_TYPING;
    drawTyping(); // the first frame, while still hidden
    if (tyState == TY_OFF) return;
    typingShow();
    animTimer();
}
// The plain banner's text: the typing line, or "wasdmod off  -  Backtick to turn it on".
static void plainBannerText(char *out, unsigned cap) {
    out[0] = 0;
    if (bnKind == BN_LAYOUT) { char name[64]; layoutName(name, sizeof(name)); append(out, cap, T("Key layout loaded")); append(out, cap, "  -  "); append(out, cap, name); return; }
    if (bnKind != BN_OFF) { append(out, cap, T("TYPING  -  Esc to finish")); return; }
    char key[32], line[LINE_BYTES]; nthKey(toggleKey.text, 0, key, sizeof(key));
    fill(line, sizeof(line), T("{key} to turn it on"), "", key);
    append(out, cap, T("wasdmod off")); append(out, cap, "  -  "); append(out, cap, line);
}
static void sizeTyping(void);
// The plain banner (and, for typing, the plain frame): shown and hidden, nothing else.
static void showTypingPlain(int kind) {
    if (typeShown && kind == bnKind) return;
    if (typeShown) typingHide();
    if (kind < 0 || !gameWnd || gameWidth < 400) return;
    bnKind = kind; sizeTyping();
    typedL = gameLeft; typedT = gameTop; typedW = gameWidth; typedH = gameHeight;
    SetWindowPos(typeWnd, (HANDLE)-1, evenDown(gameLeft + (gameWidth - typeW) / 2), evenDown(gameTop + 24), typeW, typeH, 0x10 | 0x40 /*NOACTIVATE|SHOWWINDOW*/);
    if (kind == BN_TYPING) {
        frameStrips(gameLeft, gameTop, gameWidth, gameHeight, 4, frameX, frameY, frameW, frameH);
        for (int i = 0; i < 4; i++) if (frameWnd[i]) SetWindowPos(frameWnd[i], (HANDLE)-1, gameLeft + frameX[i], gameTop + frameY[i], frameW[i], frameH[i], 0x10 | 0x40);
    }
    InvalidateRect(typeWnd, 0, 1);
    typeShown = 1;
}
// Which banner should be up, every tick: "wasdmod off" while the toggle key has the
// mod switched off, else the typing banner while typing mode is on, else "Key layout
// loaded" for 4 s after a layout was loaded. That one waits until the game is in front
// and the other two have gone (with their toasts), and steps aside for the key list.
static int awayKind = -1; // the banner that was up when the game went to the background (or the key list came)
static void showTyping(int front) {
    if (!typeWnd) return;
    int kind = paused && toggleKey.vk[0] ? BN_OFF : typingManual ? BN_TYPING : -1;
    U64 now = GetTickCount64();
    if (kind >= 0 && bnKind == BN_LAYOUT && (tyState == TY_ENTER || tyState == TY_ON || (typeShown && !typeFancy)))
        { layoutPending = 1; layoutUntil = 0; } // cut short by another banner: shown again after it
    if (kind < 0 && (layoutPending || now < layoutUntil)) {
        kind = BN_LAYOUT;
        if (typeFancy && tyState != TY_OFF && bnKind != BN_LAYOUT) kind = -1; // the other banner leaves first
    }
    if (!front || ((kind == BN_OFF || kind == BN_LAYOUT) && legendWants)) {
        // The game went to the background: everything goes at once, timers stop. And
        // "wasdmod off" steps aside for the key list (F9 works with the mod off too).
        if (tyState != TY_OFF || typeShown) typingHide();
        awayKind = kind;
        return;
    }
    if (kind == BN_LAYOUT && layoutPending) {
        // Shown now (when the game window is big enough for it), for 4 s from here.
        if (!gameWnd || gameWidth < 400) return;
        layoutPending = 0; layoutUntil = now + (U64)(LAYOUT_SHOW_MS * ANIM_SLOW);
        if (typeShown && !typeFancy && bnKind == BN_LAYOUT) typingHide(); // the plain one, sized for the new name
        if (typeFancy && bnKind == BN_LAYOUT && tyState != TY_OFF) typingHide(); // leaving: comes in again
        awayKind = -1;
    }
    if (!typeFancy) { showTypingPlain(kind); awayKind = -1; return; }
    if (kind >= 0 && kind == awayKind && (tyState == TY_OFF || tyState == TY_EXIT)) { awayKind = -1; typingSettled(kind); return; }
    awayKind = -1; // what ended meanwhile gets no toast
    if (kind < 0) {
        // Esc ended typing: "Back to playing". The mod is on again: "wasdmod on".
        if (tyState == TY_ENTER || tyState == TY_ON) {
            if (caretTimer) { KillTimer(typeWnd, TYPE_CARET_TIMER); caretTimer = 0; }
            tyState = TY_EXIT; tyShaking = 0; QueryPerformanceCounter(&tyAt);
            animTimer(); drawTyping();
        }
        return;
    }
    if ((tyState == TY_ENTER || tyState == TY_ON) && bnKind != kind) typingHide(); // one straight into the other (the mod switched off while typing): no toast
    if (tyState == TY_OFF || tyState == TY_EXIT) { if (gameWnd && gameWidth >= 400) typingStart(kind); return; }
    if (kind == BN_TYPING) { // a movement key held 400 ms while typing: shake, once per hold
        static U64 heldSince; static int shook;
        if (!movementHeld()) heldSince = 0, shook = 0;
        else if (!heldSince) heldSince = GetTickCount64();
        else if (!shook && GetTickCount64() - heldSince >= 400) {
            shook = 1; tyShaking = 1; QueryPerformanceCounter(&tyShakeAt);
            logline("Typing mode: a movement key is held (banner shakes)");
            animTimer(); drawTyping();
        }
    }
    // The game window moved or changed size: the banner (and frame) follow it.
    if (gameWnd && gameWidth >= 400 && (gameLeft != typedL || gameTop != typedT || gameWidth != typedW || gameHeight != typedH)) {
        if (placeTyping()) drawTyping(); else { typingToPlain(); typingHide(); }
    }
}
// The caret's blink: one small redraw per toggle (solid while something else moves).
static void caretBlink(void) {
    if (tyState != TY_ON || bnKind != BN_TYPING) return;
    long long now; QueryPerformanceCounter(&now);
    if (CARET_BLINK_FOR_MS && perfFreq && (now - caretSince) * 1000 / perfFreq > CARET_BLINK_FOR_MS) {
        KillTimer(typeWnd, TYPE_CARET_TIMER); caretTimer = 0; caretLit = 1; drawTyping(); return;
    }
    caretLit = !caretLit;
    if (!tyShaking) drawTyping();
}
static LRESULT typeProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT: the plain banner
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        if (!typeFancy) {
            RECT r = {0, 0, typeW, typeH}; FillRect(dc, &r, bnKind == BN_OFF ? offEdge : bnKind == BN_LAYOUT ? layEdge : typeEdge);
            RECT in = {3, 3, typeW - 3, typeH - 3}; FillRect(dc, &in, typeBrush);
            SelectObject(dc, typeFont); SetBkMode(dc, 1 /*TRANSPARENT*/);
            SetTextColor(dc, bnKind == BN_OFF ? RGB(196, 180, 246) : bnKind == BN_LAYOUT ? RGB(170, 218, 240) : RGB(255, 214, 102));
            char text[LINE_BYTES]; SIZE sz; plainBannerText(text, sizeof(text));
            if (textSize(dc, text, &sz)) textOut(dc, (typeW - sz.cx) / 2, (typeH - sz.cy) / 2, text);
        }
        EndPaint(w, &ps);
        return 0;
    }
    if (msg == 0x113) { // WM_TIMER
        if (wp == TYPE_ANIM_TIMER) drawTyping();
        if (wp == TYPE_CARET_TIMER) caretBlink();
        return 0;
    }
    if (msg == 0x84) return -1; // WM_NCHITTEST: clicks go to the game
    return DefWindowProcA(w, msg, wp, lp);
}
static LRESULT frameProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x0F) { // WM_PAINT: the plain frame, solid amber
        PAINTSTRUCT ps; HANDLE dc = BeginPaint(w, &ps);
        if (!frameFancy) { RECT r; GetClientRect(w, &r); FillRect(dc, &r, frameBrush); }
        EndPaint(w, &ps);
        return 0;
    }
    if (msg == 0x84) return -1;
    return DefWindowProcA(w, msg, wp, lp);
}

// The typing banner (or a toast) has the top of the screen: the key list waits.
static int typingOnScreen(void) { return (tyState != TY_OFF && (bnKind == BN_TYPING || tyState == TY_EXIT)) || (typeShown && !typeFancy && bnKind == BN_TYPING); }

// A few seconds after the game started, the key list and the typing banner are drawn
// once in the background (one per key list tick), so the first F9 or typing mode
// mid-game doesn't load fonts then (GDI's font lock is shared with the whole game).
static void warmUp(void) {
    float s = overlayScale();
    if (cfg.legend && legendFancy && (legendDirty || legendScale != s || legendGameW != gameWidth || legendGameH != gameHeight)) {
        if (renderLegend(s, gameWidth, gameHeight)) { legendDirty = 0; legendGameW = gameWidth; legendGameH = gameHeight; }
    } else if (typeFancy && tyState == TY_OFF && !renderTypingParts(s)) typingToPlain();
}

static LRESULT tickProc(HANDLE w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == 0x113) { // WM_TIMER, every 30 ms (250 ms with the game in the background)
        ensureFilter(); checkRecording(); // here, not on the game's thread: each can wait on Wine or the Mac
        DWORD fgPid = 0; HANDLE fg = GetForegroundWindow(); if (fg) GetWindowThreadProcessId(fg, &fgPid);
        int front = gameInFront(fg);
        {   // With the game in the background there's nothing to do but notice it coming
            // back: the timer slows to 4 times a second until then.
            static int tickFast = 1;
            if (front != tickFast) { tickFast = front; SetTimer(w, 2, front ? 30 : 250, 0); }
        }
        if (!front && legendShown) legendAway(); // at once, not at the key list's next check
        showTyping(front);
        int inControl = gameWnd && active();
        // Keep the mouse inside the game while playing. The game hides its cursor in
        // controller mode, so while you move with the keys the invisible cursor is
        // quietly moved back to the middle once it has drifted far (a third of the
        // window): rare, cheap, and it can't escape to other apps. Never while a mouse button is held (drag dodges
        // measure mouse travel), and only while the game window is in front.
        int sane = gameWnd && gameWidth >= 400 && gameHeight >= 300 && fgPid == GetCurrentProcessId();
        int buttonsHeld = (GetAsyncKeyState(0x01) | GetAsyncKeyState(0x02) | GetAsyncKeyState(0x04) | GetAsyncKeyState(0x05) | GetAsyncKeyState(0x06)) & 0x8000;
        POINT p; int haveP = inControl && sane && keepCursorInside(&p); // (the cursor is read once a tick)
        if (inControl && cfg.clipCursor && sane && !buttonsHeld && movementHeld() && (haveP || GetCursorPos(&p))) {
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

// The plain panel sized to its text.
static void sizeLegend(void) {
    HANDLE dc = GetDC(0); SelectObject(dc, legendFont);
    int w = 160; SIZE s;
    if (textSize(dc, T("Keyboard controls"), &s) && s.cx > w) w = s.cx;
    for (int i = 0; i < legendCount; i++) if (textSize(dc, legendText[i], &s) && s.cx > w) w = s.cx;
    if (textSize(dc, "Ag", &s)) lineH = s.cy + 2;
    ReleaseDC(0, dc);
    legendW = evenUp(w + 20); legendH = evenUp((legendCount + 1) * lineH + 20);
}
// The plain banner sized to its text.
static void sizeTyping(void) {
    HANDLE tdc = GetDC(0); SelectObject(tdc, typeFont); SIZE ts; char text[LINE_BYTES];
    plainBannerText(text, sizeof(text));
    if (!textSize(tdc, text, &ts)) { ts.cx = 260; ts.cy = 26; }
    ReleaseDC(0, tdc);
    typeW = evenUp(ts.cx + 40); typeH = evenUp(ts.cy + 18);
}

// A layout saved or picked while the game runs (a newer settings file in the game
// folder) is used within a second: keys, options and the key list. "Key layout loaded"
// shows for 4 s so you can see it took (if the game is in the background, when it
// comes back). No restart needed.
static void reloadIfChanged(void) {
    static U64 checkedAt;
    U64 now = GetTickCount64();
    if (now - checkedAt < 1000) return;
    checkedAt = now;
    char path[1100]; DWORD when[2];
    newestSettings(path, sizeof(path), when);
    if (sameStr(path, ini) && when[0] == settingsWhen[0] && when[1] == settingsWhen[1]) return;
    int oldLang = langIndex;
    releaseRemaps();
    clearSettings();
    loadSettings();
    buildOwned();
    memset(&mouseDodge, 0, sizeof(mouseDodge));
    { unsigned n = len(ini); const char *f = ini + n; while (f > ini && f[-1] != '\\') f--;
      char l[160] = "Key layout reloaded: "; append(l, sizeof(l), f); logline(l); }
    pickLanguage();
    if (langIndex != oldLang) { // other letters: other fonts
        HANDLE lf = makeFont(-14, 500), tf = makeFont(-22, 700);
        DeleteObject(legendFont); legendFont = lf;
        if (typeFont) DeleteObject(typeFont);
        typeFont = tf; sizeTyping();
        if (typeWnd && !typeFancy) InvalidateRect(typeWnd, 0, 1);
    }
    typeDirty = 1; // the banners' language, keys or layout name may have changed
    if (typeFancy && (tyState == TY_ENTER || tyState == TY_ON)) { if (placeTyping()) drawTyping(); }
    // "Key layout loaded": already up, it shows the new name and stays 4 s from now;
    // otherwise it comes as soon as it can.
    if (bnKind == BN_LAYOUT && typeFancy && (tyState == TY_ENTER || tyState == TY_ON)) layoutUntil = now + (U64)(LAYOUT_SHOW_MS * ANIM_SLOW);
    else layoutPending = 1;
    buildLegend(); sizeLegend();
    legendDirty = 1;
    if (legendShown && !legendFancy) { ShowWindow(legendWnd, 0); legendShown = 0; } // shown again at its new size
    placeLegend();
}

static DWORD legendThread(void *unused) {
    ClipCursor(0); // an older build could leave the mouse confined
    { HANDLE nt = GetModuleHandleA("ntdll.dll"); underWine = nt && GetProcAddress(nt, "wine_get_version"); }
    buildLegend();
    measureDC = CreateCompatibleDC(0);
    WNDCLASSA wc; memset(&wc, 0, sizeof(wc));
    wc.proc = legendProc; wc.instance = self; wc.className = "WasdModLegend";
    RegisterClassA(&wc);
    legendFont = makeFont(-14, 500);
    legendBrush = CreateSolidBrush(RGB(20, 22, 28));
    sizeLegend();
    legendWnd = CreateWindowExA(0x80000 | 0x20 | 0x8 | 0x80 | 0x8000000 /*LAYERED|TRANSPARENT|TOPMOST|TOOLWINDOW|NOACTIVATE*/,
                                "WasdModLegend", "Keyboard controls", 0x80000000 /*WS_POPUP*/, 0, 0, legendW, legendH, 0, 0, self, 0);
    if (!legendWnd) { logline("Could not create the key legend"); return 0; }
    // The new look needs per-pixel alpha; without it, the plain panel.
    legendFancy = measureDC && canLayer(legendWnd);
    if (!legendFancy) { logline("Key list: plain panel (no per-pixel alpha)"); SetLayeredWindowAttributes(legendWnd, 0, 215, 2 /*LWA_ALPHA*/); }
    legendStarted = GetTickCount64();
    legendUntil = legendStarted + (U64)cfg.legendSeconds * 1000;
    SetTimer(legendWnd, 1, 250, 0);
    {   // typing banner and frame
        WNDCLASSA tc; memset(&tc, 0, sizeof(tc));
        tc.proc = typeProc; tc.instance = self; tc.className = "WasdModTyping";
        RegisterClassA(&tc);
        typeFont = makeFont(-22, 700);
        typeBrush = CreateSolidBrush(RGB(20, 22, 28)); typeEdge = CreateSolidBrush(RGB(255, 179, 64)); offEdge = CreateSolidBrush(RGB(166, 140, 240));
        layEdge = CreateSolidBrush(RGB(91, 181, 224));
        frameBrush = CreateSolidBrush(RGB(255, 179, 64));
        sizeTyping();
        typeWnd = CreateWindowExA(0x80000 | 0x20 | 0x8 | 0x80 | 0x8000000 /*LAYERED|TRANSPARENT|TOPMOST|TOOLWINDOW|NOACTIVATE*/,
                                  "WasdModTyping", "Typing mode", 0x80000000 /*WS_POPUP*/, 0, 0, typeW, typeH, 0, 0, self, 0);
        WNDCLASSA fc; memset(&fc, 0, sizeof(fc));
        fc.proc = frameProc; fc.instance = self; fc.className = "WasdModFrame";
        RegisterClassA(&fc);
        for (int i = 0; i < 4 && typeWnd; i++)
            frameWnd[i] = CreateWindowExA(0x80000 | 0x20 | 0x8 | 0x80 | 0x8000000, "WasdModFrame", "Typing mode frame", 0x80000000, 0, 0, 4, 4, 0, 0, self, 0);
        typeFancy = typeWnd && measureDC && canLayer(typeWnd);
        frameFancy = typeFancy && frameWnd[0] && canLayer(frameWnd[0]);
        if (typeWnd && !typeFancy) SetLayeredWindowAttributes(typeWnd, 0, 230, 2 /*LWA_ALPHA*/);
        for (int i = 0; i < 4; i++) if (frameWnd[i] && !frameFancy) SetLayeredWindowAttributes(frameWnd[i], 0, FRAME_ALPHA, 2);
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
