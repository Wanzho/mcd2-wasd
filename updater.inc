// The Windows setup's one-click update (GitHub build only: installer.c includes this
// file unless it's built with -DNEXUS, and only then links WinHTTP).
//
// Checking reads GitHub's "latest release" (REL_API). Updating downloads that
// release's setup (wasdmod-VERSION.exe; wasdmod-Windows.exe before 1.4) and
// SHA256SUMS with WinHTTP, only from this repository's release files and GitHub's own
// download hosts (release.inc), checks the file against its checksum, renames the
// running exe to NEW.old, writes the new one (in this one's place; named for the new
// version when this one is named for its own) and starts it with /updated; the new
// one deletes NEW.old. A file
// written this way has no "downloaded from the internet" mark (no Zone.Identifier
// stream), so SmartScreen doesn't stop it the way it stops a browser download.
//
// Nothing here runs by itself except the daily check: downloading starts only from
// the Update now button.
#ifndef NEXUS
#include "release.inc"

// winhttp
IMP HANDLE WinHttpOpen(const WCHAR *, DWORD, const WCHAR *, const WCHAR *, DWORD);
IMP HANDLE WinHttpConnect(HANDLE, const WCHAR *, WORD, DWORD);
IMP HANDLE WinHttpOpenRequest(HANDLE, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR **, DWORD);
IMP BOOL WinHttpSendRequest(HANDLE, const WCHAR *, DWORD, void *, DWORD, DWORD, U64);
IMP BOOL WinHttpReceiveResponse(HANDLE, void *);
IMP BOOL WinHttpQueryHeaders(HANDLE, DWORD, const WCHAR *, void *, DWORD *, DWORD *);
IMP BOOL WinHttpReadData(HANDLE, void *, DWORD, DWORD *);
IMP BOOL WinHttpSetOption(HANDLE, DWORD, void *, DWORD);
IMP BOOL WinHttpSetTimeouts(HANDLE, int, int, int, int);
IMP BOOL WinHttpCloseHandle(HANDLE);
// kernel32 (wide paths: the exe may sit under a user name outside the ANSI code page)
IMP DWORD GetModuleFileNameW(HANDLE, WCHAR *, DWORD);
IMP BOOL MoveFileExW(const WCHAR *, const WCHAR *, DWORD);
IMP HANDLE CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
IMP BOOL DeleteFileW(const WCHAR *);
IMP DWORD GetFileAttributesW(const WCHAR *);
IMP DWORD GetEnvironmentVariableW(const WCHAR *, WCHAR *, DWORD);
typedef struct { DWORD cb; WCHAR *reserved, *desktop, *title; DWORD x, y, w, h, cx, cy, fill, flags; WORD show, reserved2; BYTE *reserved3; HANDLE in, out, err; } STARTUPINFOW;
typedef struct { HANDLE process, thread; DWORD pid, tid; } PROCESSINFO;
IMP BOOL CreateProcessW(const WCHAR *, WCHAR *, void *, void *, BOOL, DWORD, void *, const WCHAR *, STARTUPINFOW *, PROCESSINFO *);

#define WM_UPD_CHECKED 0x8003    // the check is done: wParam 1 if it was the button, lParam the result
#define WM_UPD_PROGRESS 0x8004   // wParam: percent downloaded, or 101 while the file is checked
#define WM_UPD_DOWNLOADED 0x8005 // wParam: the result

enum { UPD_OK, UPD_OFFLINE, UPD_RATE, UPD_HTTP, UPD_REFUSED, UPD_TOOBIG, UPD_CANCELLED, UPD_NOTRELEASE,
       UPD_NOFILE, UPD_NOSUMS, UPD_BADSUM, UPD_BADFILE, UPD_READONLY, UPD_WRITE, UPD_LAUNCH };
static const char *const updErrorText[] = { // translated where they're shown
    "",
    N_("Couldn't reach GitHub. Check the internet connection and try again."),
    N_("GitHub is limiting update checks right now. Try again in an hour."),
    N_("GitHub answered with an error. Try again later."),
    N_("The download was redirected to an address outside GitHub, so wasdmod stopped it."),
    N_("The download is much bigger than it should be, so wasdmod stopped it."),
    N_("Update cancelled."),
    N_("GitHub's answer wasn't a wasdmod release."),
    N_("This release has no Windows download."),
    N_("This release has no checksum file (SHA256SUMS), so wasdmod can't check the download."),
    N_("The download doesn't match its checksum, so wasdmod didn't use it."),
    N_("The download isn't the right setup, so wasdmod didn't use it."),
    N_("wasdmod can't write to this copy's folder."),
    N_("Couldn't write the new version next to this one."),
    N_("Couldn't start the new version, so wasdmod kept this one."),
};

static const char *updApi = REL_API;
static HANDLE updNotify;         // the window that hears about progress
static volatile int updCancel;   // Cancel during the download
static RELINFO updRelease;       // the newest release, once checked
static char *updFile; static DWORD updFileN; // the checked new setup, in memory
static WCHAR updSelf[PATHLEN];   // this exe

static void updWide(const char *s, WCHAR *w, int cap) { if (!MultiByteToWideChar(65001, 0, s, -1, w, cap)) w[0] = 0; }
static void wcopy(WCHAR *d, unsigned cap, const WCHAR *s) { unsigned i = 0; while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; } d[i] = 0; }
static void wcat(WCHAR *d, unsigned cap, const WCHAR *s) { unsigned n = 0; while (d[n]) n++; if (n < cap) wcopy(d + n, cap - n, s); }
static void updFree(char **b) { if (*b) HeapFree(GetProcessHeap(), 0, *b); *b = 0; }

#ifdef SELFTEST
// Test builds only: WASDMOD_TEST_API (the "latest release" address) and
// WASDMOD_TEST_DOWNLOADS (in place of the repository's download address) point the
// updater at a local test server, over plain http.
static void updTestSetup(void) {
    static char api[600], downloads[600], host[100];
    if (GetEnvironmentVariableA("WASDMOD_TEST_API", api, sizeof(api))) {
        updApi = api;
        unsigned k = 0; const char *h = api + (relStarts(api, "http://") ? 7 : 8);
        while (h[k] && h[k] != '/' && k + 1 < sizeof(host)) { host[k] = h[k]; k++; }
        host[k] = 0; relTestHost = host;
    }
    if (GetEnvironmentVariableA("WASDMOD_TEST_DOWNLOADS", downloads, sizeof(downloads))) relDownloads = downloads;
}
#endif

static HANDLE updSession;
static int updOpen(void) {
    if (updSession) return 1;
    WCHAR agent[60]; updWide("wasdmod-setup/" VERSION, agent, 60);
    updSession = WinHttpOpen(agent, 4 /*WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY*/, 0, 0, 0);
    if (!updSession) updSession = WinHttpOpen(agent, 0 /*the system's proxy setting*/, 0, 0, 0);
    if (updSession) WinHttpSetTimeouts(updSession, 15000, 15000, 20000, 30000);
    return updSession != 0;
}

// The answer's body, into a buffer of at most `limit` bytes; with `report`, the
// window hears the percentage as it changes.
static int updRead(HANDLE req, char **out, DWORD *outN, DWORD limit, int report) {
    DWORD total = 0, size = 4, n = 0, cap;
    if (!WinHttpQueryHeaders(req, 5 /*CONTENT_LENGTH*/ | 0x20000000 /*FLAG_NUMBER*/, 0, &total, &size, 0)) total = 0;
    if (total > limit) return UPD_TOOBIG;
    cap = total ? total + 1 : 1 << 16;
    char *b = HeapAlloc(GetProcessHeap(), 0, cap);
    int lastPct = -1;
    if (!b) return UPD_TOOBIG;
    for (;;) {
        if (updCancel) { updFree(&b); return UPD_CANCELLED; }
        if (n + 1 >= cap) { // bigger than it said, or it didn't say
            DWORD more = cap * 2 > limit + 1 ? limit + 1 : cap * 2;
            if (more <= cap) { updFree(&b); return UPD_TOOBIG; }
            char *nb = HeapAlloc(GetProcessHeap(), 0, more);
            if (!nb) { updFree(&b); return UPD_TOOBIG; }
            memcpy(nb, b, n); HeapFree(GetProcessHeap(), 0, b); b = nb; cap = more;
        }
        DWORD got = 0;
        if (!WinHttpReadData(req, b + n, cap - n - 1, &got)) { updFree(&b); return UPD_OFFLINE; }
        if (!got) break;
        n += got;
        if (n > limit) { updFree(&b); return UPD_TOOBIG; }
        int pct = total ? (int)((unsigned long long)n * 100 / total) : 0;
        if (report && pct != lastPct && pct <= 100) { lastPct = pct; PostMessageA(updNotify, WM_UPD_PROGRESS, (WPARAM)pct, 0); }
    }
    b[n] = 0; *out = b; *outN = n;
    return UPD_OK;
}

// GET `start`, following GitHub's redirects only to its own hosts (WinHTTP's own
// redirect handling is off, so each address is checked here first).
static int updGet(const char *start, char **out, DWORD *outN, DWORD limit, int report) {
    static char url[REL_URLLEN], next[REL_URLLEN];
    static WCHAR wpath[REL_URLLEN], wloc[REL_URLLEN];
    int api = relSame(start, updApi);
    relCopy(url, sizeof(url), start);
    *out = 0; *outN = 0;
    if (!updOpen()) return UPD_OFFLINE;
    for (int hop = 0; hop < 6; hop++) {
        unsigned hn; const char *h = relHost(url, &hn);
        if (!h || !relAllowedHop(url)) return UPD_REFUSED;
        int secure = relStarts(url, "https://");
        char hostA[256]; unsigned k = 0; WORD port = secure ? 443 : 80;
        while (k < hn && h[k] != ':' && k + 1 < sizeof(hostA)) { hostA[k] = h[k]; k++; }
        hostA[k] = 0;
        if (k < hn && h[k] == ':') { port = 0; for (unsigned i = k + 1; i < hn; i++) port = (WORD)(port * 10 + (h[i] - '0')); }
        WCHAR host[256]; updWide(hostA, host, 256);
        updWide(h[hn] ? h + hn : "/", wpath, REL_URLLEN);
        HANDLE con = WinHttpConnect(updSession, host, port, 0), req = 0;
        if (con) req = WinHttpOpenRequest(con, L"GET", wpath, 0, 0, 0, (secure ? 0x00800000 /*SECURE*/ : 0) | 0x40 | 0x80 /*already escaped*/);
        DWORD off = 2 /*WINHTTP_DISABLE_REDIRECTS*/;
        if (req) WinHttpSetOption(req, 63 /*WINHTTP_OPTION_DISABLE_FEATURE*/, &off, 4);
        const WCHAR *head = api ? L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n" : L"Accept: application/octet-stream\r\n";
        int r = UPD_OFFLINE;
        if (req && WinHttpSendRequest(req, head, (DWORD)-1, 0, 0, 0, 0) && WinHttpReceiveResponse(req, 0)) {
            DWORD status = 0, size = 4;
            WinHttpQueryHeaders(req, 19 /*STATUS_CODE*/ | 0x20000000, 0, &status, &size, 0);
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                DWORD ls = sizeof(wloc);
                r = UPD_REFUSED;
                if (WinHttpQueryHeaders(req, 33 /*LOCATION*/, 0, wloc, &ls, 0) && WideCharToMultiByte(65001, 0, wloc, -1, next, sizeof(next), 0, 0) > 0) {
                    if (next[0] == '/' && next[1] != '/') { // a path on the same host
                        char base[REL_URLLEN]; unsigned b = (unsigned)(h + hn - url);
                        relCopy(base, b + 1 < sizeof(base) ? b + 1 : sizeof(base), url); relCat(base, sizeof(base), next); relCopy(next, sizeof(next), base);
                    }
                    relCopy(url, sizeof(url), next);
                    r = -1; // follow it
                }
            } else if (status == 200) r = updRead(req, out, outN, limit, report);
            else r = api && (status == 403 || status == 429) ? UPD_RATE : UPD_HTTP;
        }
        if (req) WinHttpCloseHandle(req);
        if (con) WinHttpCloseHandle(con);
        if (r != -1) return r;
    }
    return UPD_REFUSED; // redirected in circles
}

// ---------------------------------------------------------------- checking

// A file of the updater's in %APPDATA%\wasdmod (with the editor's layouts).
static void updAppFile(const char *name, char *out) {
    char dir[PATHLEN] = ""; out[0] = 0;
    if (!GetEnvironmentVariableA("APPDATA", dir, 300)) return;
    cat(dir, PATHLEN, "\\wasdmod"); CreateDirectoryA(dir, 0);
    join(out, dir, name);
}
// The daily check: last-update-check's date is the last time GitHub answered.
static void updCheckedFile(char *out) { updAppFile("last-update-check", out); }
static int updDue(void) {
    char p[PATHLEN]; FILEINFO f; DWORD now[2];
    updCheckedFile(p);
    if (!p[0] || !GetFileAttributesExA(p, 0, &f)) return 1;
    GetSystemTimeAsFileTime(now);
    U64 a = (U64)f.written[1] << 32 | f.written[0], b = (U64)now[1] << 32 | now[0];
    return b < a || b - a > 864000000000ULL; // a day, in 100 ns steps (or the clock went back)
}
static DWORD updCheckThread(void *manual) {
    char *b; DWORD n;
    int r = updGet(updApi, &b, &n, 4 << 20, 0);
    if (r == UPD_OK) { r = relParse(b, n, "wasdmod-", ".exe", &updRelease) ? UPD_OK : UPD_NOTRELEASE; updFree(&b); }
    if (r != UPD_OFFLINE) { char p[PATHLEN]; updCheckedFile(p); if (p[0]) writeAll(p, VERSION, slen(VERSION)); } // GitHub answered
    PostMessageA(updNotify, WM_UPD_CHECKED, (WPARAM)manual, (LPARAM)r);
    return 0;
}
static void updStartCheck(int manual) {
    HANDLE t = CreateThread(0, 0, updCheckThread, (void *)(U64)manual, 0, 0);
    if (t) CloseHandle(t); else PostMessageA(updNotify, WM_UPD_CHECKED, (WPARAM)manual, UPD_OFFLINE);
}

// ---------------------------------------------------------------- downloading

static int updDownload(void) {
    RELINFO *rel = &updRelease;
    char *sums = 0, want[65], got[65], mark[80] = "wasdmod setup for Windows ";
    DWORD sn = 0;
    updFree(&updFile);
    if (!rel->asset[0] || !relAllowedStart(rel->asset)) return UPD_NOFILE;
    if (!rel->sums[0] || !relAllowedStart(rel->sums)) return UPD_NOSUMS;
    int r = updGet(rel->sums, &sums, &sn, 64 << 10, 0);
    if (r) return r;
    int listed = relSumFor(sums, sn, rel->assetName, want);
    updFree(&sums);
    if (!listed) return UPD_NOSUMS;
    r = updGet(rel->asset, &updFile, &updFileN, 64 << 20, 1);
    if (r) return r;
    PostMessageA(updNotify, WM_UPD_PROGRESS, 101, 0); // checking it
    relSha256((const unsigned char *)updFile, updFileN, got);
    if (!relSame(got, want)) { updFree(&updFile); return UPD_BADSUM; }
    // And it's the setup of that version (its report line names it).
    relCat(mark, sizeof(mark), rel->version); relCat(mark, sizeof(mark), "\n");
    if (updFileN < 4096 || updFile[0] != 'M' || updFile[1] != 'Z' || !contains(updFile, updFileN, mark)) { updFree(&updFile); return UPD_BADFILE; }
    return UPD_OK;
}
static DWORD updDownloadThread(void *arg) {
    (void)arg;
    PostMessageA(updNotify, WM_UPD_DOWNLOADED, (WPARAM)updDownload(), 0);
    return 0;
}
static void updStartDownload(void) {
    updCancel = 0;
    HANDLE t = CreateThread(0, 0, updDownloadThread, 0, 0, 0);
    if (t) CloseHandle(t); else PostMessageA(updNotify, WM_UPD_DOWNLOADED, UPD_WRITE, 0);
}

// ---------------------------------------------------------------- replacing this exe

static WCHAR updNew[PATHLEN];    // where the new setup goes
static void updOldName(const WCHAR *exe, WCHAR *out) { wcopy(out, PATHLEN + 8, exe); wcat(out, PATHLEN + 8, L".old"); }
static int wsameText(const WCHAR *a, const WCHAR *b) {
    for (; *a && *b; a++, b++) { WCHAR x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a, y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b; if (x != y) return 0; }
    return *a == *b;
}
// The new setup takes this exe's place. Named for its version (wasdmod-1.3.1.exe, as
// downloaded), it's named for the new one (wasdmod-1.4.0.exe), so the name never shows
// the old version after an update; a name of the player's own stays.
static void updNewName(void) {
    WCHAR mine[60], next[60]; char a[60] = "wasdmod-";
    WCHAR *base = updNew;
    wcopy(updNew, PATHLEN, updSelf);
    for (WCHAR *q = updNew; *q; q++) if (*q == '\\' || *q == '/') base = q + 1;
    updWide("wasdmod-" VERSION ".exe", mine, 60);
    relCat(a, sizeof(a), updRelease.version); relCat(a, sizeof(a), ".exe"); updWide(a, next, 60);
    if (wsameText(base, mine) && (unsigned)(base - updNew) + 60 < PATHLEN) wcopy(base, 60, next);
}
static int updWriteFile(const WCHAR *path, const char *d, DWORD n) {
    HANDLE f = CreateFileW(path, 0x40000000 /*GENERIC_WRITE*/, 0, 0, 1 /*CREATE_NEW*/, 0x80, 0);
    if (f == (HANDLE)-1) return 0;
    DWORD put = 0; BOOL ok = WriteFile(f, d, n, &put, 0) && put == n;
    CloseHandle(f);
    if (!ok) DeleteFileW(path);
    return ok;
}
// The running exe can't be overwritten, but it can be renamed: it becomes NEW.old
// (which the new setup deletes, as its own name + .old) and the new setup is written
// in its place, wherever the player keeps it.
static int updReplaceSelf(void) {
    WCHAR old[PATHLEN + 8];
    if (!GetModuleFileNameW(0, updSelf, PATHLEN) || !updFile) return UPD_WRITE;
    updNewName();
    updOldName(updNew, old);
    DeleteFileW(old); // left from an earlier update
    if (!MoveFileExW(updSelf, old, 1 /*REPLACE_EXISTING*/)) return UPD_READONLY;
    if (!wsameText(updNew, updSelf) && GetFileAttributesW(updNew) != 0xFFFFFFFF) DeleteFileW(updNew); // that version, downloaded by hand
    if (!updWriteFile(updNew, updFile, updFileN)) { MoveFileExW(old, updSelf, 0); return UPD_WRITE; }
    return UPD_OK;
}
static void updUndoReplace(void) { WCHAR old[PATHLEN + 8]; updOldName(updNew, old); DeleteFileW(updNew); MoveFileExW(old, updSelf, 0); }
// Starts the new setup: /updated "<game folder>" (it keeps a folder picked with Browse).
static int updRelaunch(const char *dir) {
    static WCHAR cmd[PATHLEN * 3];
    WCHAR wdir[PATHLEN];
    if (!MultiByteToWideChar(0 /*CP_ACP, like gameDir*/, 0, dir, -1, wdir, PATHLEN)) wdir[0] = 0;
    wcopy(cmd, PATHLEN * 3, L"\""); wcat(cmd, PATHLEN * 3, updNew); wcat(cmd, PATHLEN * 3, L"\" /updated \"");
    wcat(cmd, PATHLEN * 3, wdir); wcat(cmd, PATHLEN * 3, L"\"");
    STARTUPINFOW si; PROCESSINFO pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    if (!CreateProcessW(updNew, cmd, 0, 0, 0, 0, 0, 0, &si, &pi)) return UPD_LAUNCH;
    CloseHandle(pi.process); CloseHandle(pi.thread);
    return UPD_OK;
}
// When this exe's folder can't be changed: the checked new setup goes to Downloads.
static int updSaveToDownloads(WCHAR *out) {
    WCHAR name[80];
    out[0] = 0;
#ifdef SELFTEST
    if (!GetEnvironmentVariableW(L"WASDMOD_TEST_SAVE_DIR", out, PATHLEN - 100)) out[0] = 0; // tests: not the real Downloads
#endif
    if (!out[0]) {
        if (!GetEnvironmentVariableW(L"USERPROFILE", out, PATHLEN - 100)) return 0;
        wcat(out, PATHLEN, L"\\Downloads");
    }
    wcat(out, PATHLEN, L"\\wasdmod-");
    updWide(updRelease.version, name, 40); wcat(out, PATHLEN, name); wcat(out, PATHLEN, L".exe");
    if (GetFileAttributesW(out) != 0xFFFFFFFF) DeleteFileW(out);
    return updWriteFile(out, updFile, updFileN);
}
// After an update: NAME.old goes once the old setup has quit (it may take a moment).
static DWORD updCleanThread(void *arg) {
    WCHAR old[PATHLEN + 8];
    if (!GetModuleFileNameW(0, updSelf, PATHLEN)) return 0;
    updOldName(updSelf, old);
    for (int i = 0; i < (arg ? 100 : 1) && GetFileAttributesW(old) != 0xFFFFFFFF; i++) { if (DeleteFileW(old)) break; Sleep(100); }
    return 0;
}

// What the old setup tells the new one: the version it updated to and the notes' summary.
static void updMarkerFile(char *out) { updAppFile("updated.txt", out); }
static void updWriteMarker(void) {
    static char m[800]; char p[PATHLEN];
    scpy(m, sizeof(m), "to="); cat(m, sizeof(m), updRelease.version); cat(m, sizeof(m), "\nfrom=" VERSION "\nsummary=");
    cat(m, sizeof(m), updRelease.summary); cat(m, sizeof(m), "\n");
    updMarkerFile(p); if (p[0]) writeAll(p, m, slen(m));
}
// The marker, if it's for this version: fills in the summary, and deletes it.
static int updReadMarker(char *summary, unsigned cap) {
    char p[PATHLEN]; DWORD n = 0; char *b; int ours = 0;
    summary[0] = 0;
    updMarkerFile(p); if (!p[0] || !(b = readAll(p, &n))) return 0;
    for (char *line = b; *line; ) {
        char *end = line; while (*end && *end != '\n') end++;
        char c = *end; *end = 0;
        if (sameTextN(line, "to=", 3)) ours = relSame(line + 3, VERSION);
        if (sameTextN(line, "summary=", 8)) scpy(summary, cap, line + 8);
        *end = c; line = c ? end + 1 : end;
    }
    HeapFree(GetProcessHeap(), 0, b);
    DeleteFileA(p);
    return ours;
}
#endif // NEXUS
