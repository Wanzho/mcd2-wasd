// The update check's parts that don't touch Windows (installer.c's updater.inc uses
// them; release_test.c tests them on the Mac): reading GitHub's "latest release"
// JSON, comparing versions, SHA-256, the release's SHA256SUMS file, which addresses
// a download may come from, and a one-line summary of the release notes.
//
// No C runtime: the includer provides memset/memcpy (installer.c) or libc (the test).

#define REL_API "https://api.github.com/repos/Wanzho/mcd2-wasd/releases/latest"
#define REL_DOWNLOADS "https://github.com/Wanzho/mcd2-wasd/releases/download/"
#define REL_PAGES "https://github.com/Wanzho/mcd2-wasd/releases/"
#define REL_URLLEN 1600 // asset redirects carry long signed query strings

typedef struct {
    char tag[40], version[40]; // "v1.4.0", "1.4.0"
    char page[300];            // the release's page on GitHub (only ever REL_PAGES...)
    char summary[480];         // "Drag keys around · Clearer settings · ..."
    char asset[REL_URLLEN];    // the download for this system, if the release has it
    char assetName[200];       // its name ("wasdmod-1.4.0.exe"), as SHA256SUMS lists it
    char sums[REL_URLLEN];     // its SHA256SUMS file, if the release has one
} RELINFO;

static unsigned relLen(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static int relStarts(const char *s, const char *prefix) { while (*prefix) if (*s++ != *prefix++) return 0; return 1; }
static int relSame(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static void relCopy(char *d, unsigned cap, const char *s) { unsigned i = 0; while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; } d[i] = 0; }
static void relCat(char *d, unsigned cap, const char *s) { unsigned n = relLen(d); if (n < cap) relCopy(d + n, cap - n, s); }

// ---------------------------------------------------------------- versions

// "1.10.0" > "1.9.2"; a leading "v" is ignored. A version without numbers ("dev",
// a test build) is never out of date.
static int relNewer(const char *a, const char *b) {
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    if (*b < '0' || *b > '9') return 0;
    for (int part = 0; part < 6; part++) {
        unsigned x = 0, y = 0;
        while (*a >= '0' && *a <= '9') x = x * 10 + (unsigned)(*a++ - '0');
        while (*b >= '0' && *b <= '9') y = y * 10 + (unsigned)(*b++ - '0');
        if (x != y) return x > y;
        if (*a == '.') a++; else a = "";
        if (*b == '.') b++; else b = "";
        if (!*a && !*b) break;
    }
    return 0;
}
// A plain version: digits and dots only, like 1.4.0.
static int relIsVersion(const char *v) {
    if (*v < '0' || *v > '9') return 0;
    for (; *v; v++) if ((*v < '0' || *v > '9') && *v != '.') return 0;
    return 1;
}

// ---------------------------------------------------------------- addresses

// Where a download may start: this repository's release files on github.com.
static const char *relDownloads = REL_DOWNLOADS;
// Where GitHub may redirect it: its own hosts, over https.
static const char *const REL_HOSTS[] = {"github.com", "api.github.com", "objects.githubusercontent.com",
                                        "release-assets.githubusercontent.com", "github-releases.githubusercontent.com"};
static const char *relTestHost; // a test build's local server ("127.0.0.1:8123"), else 0
static int relHostIs(const char *h, unsigned n, const char *want) {
    unsigned i = 0;
    for (; i < n && want[i]; i++) { char c = h[i] >= 'A' && h[i] <= 'Z' ? h[i] + 32 : h[i]; if (c != want[i]) return 0; }
    return i == n && !want[i];
}
// The host part of "https://host/..." (its length in *n), or 0 when the scheme isn't allowed.
static const char *relHost(const char *url, unsigned *n) {
    const char *h;
    if (relStarts(url, "https://")) h = url + 8;
    else if (relTestHost && relStarts(url, "http://")) h = url + 7;
    else return 0;
    unsigned k = 0;
    while (h[k] && h[k] != '/' && h[k] != '?' && h[k] != '#') {
        if (h[k] == '@' || h[k] == '\\' || h[k] == ' ') return 0; // no user:password@, no tricks
        k++;
    }
    *n = k;
    return k ? h : 0;
}
static int relAllowedHop(const char *url) {
    unsigned n; const char *h = relHost(url, &n);
    if (!h) return 0;
    if (relTestHost) return relHostIs(h, n, relTestHost);
    for (unsigned i = 0; i < sizeof(REL_HOSTS) / sizeof(*REL_HOSTS); i++) if (relHostIs(h, n, REL_HOSTS[i])) return 1;
    return 0;
}
static int relAllowedStart(const char *url) {
    if (!relStarts(url, relDownloads)) return 0;
    for (const char *p = url + relLen(relDownloads); *p; p++) if (*p == '?' || *p == '#' || *p == '\\' || (p[0] == '.' && p[1] == '.')) return 0;
    return relAllowedHop(url);
}

// ---------------------------------------------------------------- JSON

typedef struct { const char *s; unsigned n, i; } RELJSON;
static void jsSpace(RELJSON *j) { while (j->i < j->n && (j->s[j->i] == ' ' || j->s[j->i] == '\n' || j->s[j->i] == '\r' || j->s[j->i] == '\t')) j->i++; }
static int jsHex4(RELJSON *j, unsigned *v) {
    *v = 0;
    for (int k = 0; k < 4; k++) {
        if (j->i >= j->n) return 0;
        char c = j->s[j->i++]; int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0;
        *v = *v << 4 | (unsigned)d;
    }
    return 1;
}
// A string (at its opening quote) into out as UTF-8 (cut to fit `cap`), or skipped (out 0).
static int jsString(RELJSON *j, char *out, unsigned cap) {
    jsSpace(j);
    if (j->i >= j->n || j->s[j->i] != '"') return 0;
    j->i++;
    unsigned k = 0;
    while (j->i < j->n && j->s[j->i] != '"') {
        unsigned c = (unsigned char)j->s[j->i++];
        if (c == '\\') {
            if (j->i >= j->n) return 0;
            char e = j->s[j->i++];
            if (e == 'n') c = '\n'; else if (e == 'r') c = '\r'; else if (e == 't') c = '\t'; else if (e == 'b') c = 8; else if (e == 'f') c = 12;
            else if (e == 'u') {
                if (!jsHex4(j, &c)) return 0;
                if (c >= 0xD800 && c < 0xDC00 && j->i + 1 < j->n && j->s[j->i] == '\\' && j->s[j->i + 1] == 'u') {
                    unsigned lo; j->i += 2;
                    if (!jsHex4(j, &lo)) return 0;
                    c = lo >= 0xDC00 && lo < 0xE000 ? 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00) : 0xFFFD;
                } else if (c >= 0xD800 && c < 0xE000) c = 0xFFFD;
                if (c >= 0x80) { // as UTF-8
                    char u[4]; int m;
                    if (c < 0x800) { u[0] = (char)(0xC0 | c >> 6); u[1] = (char)(0x80 | (c & 63)); m = 2; }
                    else if (c < 0x10000) { u[0] = (char)(0xE0 | c >> 12); u[1] = (char)(0x80 | (c >> 6 & 63)); u[2] = (char)(0x80 | (c & 63)); m = 3; }
                    else { u[0] = (char)(0xF0 | c >> 18); u[1] = (char)(0x80 | (c >> 12 & 63)); u[2] = (char)(0x80 | (c >> 6 & 63)); u[3] = (char)(0x80 | (c & 63)); m = 4; }
                    if (out && k + (unsigned)m < cap) for (int q = 0; q < m; q++) out[k++] = u[q];
                    continue;
                }
            } else c = (unsigned char)e; // \" \\ \/
        }
        if (out && k + 1 < cap) out[k++] = (char)c;
    }
    if (j->i >= j->n) return 0;
    j->i++;
    if (out) out[k] = 0;
    return 1;
}
// Any value (skipped).
static int jsSkip(RELJSON *j, int depth) {
    jsSpace(j);
    if (j->i >= j->n || depth > 40) return 0;
    char c = j->s[j->i];
    if (c == '"') return jsString(j, 0, 0);
    if (c == '{' || c == '[') {
        char close = c == '{' ? '}' : ']';
        j->i++; jsSpace(j);
        if (j->i < j->n && j->s[j->i] == close) { j->i++; return 1; }
        for (;;) {
            if (c == '{') { if (!jsString(j, 0, 0)) return 0; jsSpace(j); if (j->i >= j->n || j->s[j->i++] != ':') return 0; }
            if (!jsSkip(j, depth + 1)) return 0;
            jsSpace(j);
            if (j->i >= j->n) return 0;
            if (j->s[j->i] == ',') { j->i++; continue; }
            if (j->s[j->i] == close) { j->i++; return 1; }
            return 0;
        }
    }
    while (j->i < j->n && j->s[j->i] != ',' && j->s[j->i] != '}' && j->s[j->i] != ']' && j->s[j->i] != ' ' && j->s[j->i] != '\n' && j->s[j->i] != '\r' && j->s[j->i] != '\t') j->i++;
    return 1; // a number, true, false or null
}
// Steps into an object: 1 at its first key, 0 when it's empty or not an object.
static int jsEnter(RELJSON *j, char open) {
    jsSpace(j);
    if (j->i >= j->n || j->s[j->i] != open) return -1;
    j->i++; jsSpace(j);
    if (j->i < j->n && j->s[j->i] == (open == '{' ? '}' : ']')) { j->i++; return 0; }
    return 1;
}
// After a value: 1 when another follows, 0 at the end, -1 when it's broken.
static int jsNext(RELJSON *j, char close) {
    jsSpace(j);
    if (j->i >= j->n) return -1;
    if (j->s[j->i] == ',') { j->i++; return 1; }
    if (j->s[j->i] == close) { j->i++; return 0; }
    return -1;
}
static int jsKey(RELJSON *j, char *key, unsigned cap) {
    if (!jsString(j, key, cap)) return 0;
    jsSpace(j);
    return j->i < j->n && j->s[j->i++] == ':';
}

static void relSummary(const char *notes, char *out, unsigned cap);
static char relNotes[16384]; // static: no stack probe without the C runtime

// A release file for this system: PREFIX*SUFFIX ("wasdmod-*.exe"), a plain name.
static int relAssetLike(const char *name, const char *prefix, const char *suffix) {
    unsigned n = relLen(name), p = relLen(prefix), x = relLen(suffix);
    if (n <= p + x || !relStarts(name, prefix) || !relSame(name + n - x, suffix)) return 0;
    for (const char *c = name; *c; c++) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '.' || *c == '-' || *c == '_')) return 0;
    return 1;
}
#define REL_CANDIDATES 8
static char relCandName[REL_CANDIDATES][200], relCandUrl[REL_CANDIDATES][REL_URLLEN];

// GitHub's "latest release" answer: the version, its page, the notes' summary, and
// among its files SHA256SUMS and the download for this system: PREFIX*SUFFIX, by
// preference PREFIX<version>SUFFIX ("wasdmod-1.4.0.exe"; older releases had
// "wasdmod-Windows.exe"). Empty when it has none; 0 when it isn't a release.
static int relParse(const char *s, unsigned n, const char *prefix, const char *suffix, RELINFO *r) {
    RELJSON j = {s, n, 0};
    char key[32];
    int candidates = 0;
    memset(r, 0, sizeof(*r));
    relNotes[0] = 0;
    int more = jsEnter(&j, '{');
    while (more == 1) {
        if (!jsKey(&j, key, sizeof(key))) return 0;
        int ok;
        if (relSame(key, "tag_name")) ok = jsString(&j, r->tag, sizeof(r->tag));
        else if (relSame(key, "html_url")) ok = jsString(&j, r->page, sizeof(r->page));
        else if (relSame(key, "body")) ok = jsString(&j, relNotes, sizeof(relNotes)) || jsSkip(&j, 1);
        else if (relSame(key, "assets")) {
            int a = jsEnter(&j, '[');
            ok = a >= 0;
            while (a == 1) {
                static char name[200], url[REL_URLLEN];
                name[0] = url[0] = 0;
                int f = jsEnter(&j, '{');
                if (f < 0) return 0;
                while (f == 1) {
                    if (!jsKey(&j, key, sizeof(key))) return 0;
                    if (relSame(key, "name")) { if (!jsString(&j, name, sizeof(name))) return 0; }
                    else if (relSame(key, "browser_download_url")) { if (!jsString(&j, url, sizeof(url))) return 0; }
                    else if (!jsSkip(&j, 2)) return 0;
                    f = jsNext(&j, '}');
                }
                if (f < 0) return 0;
                if (relAssetLike(name, prefix, suffix) && url[0] && candidates < REL_CANDIDATES) {
                    relCopy(relCandName[candidates], sizeof(relCandName[0]), name);
                    relCopy(relCandUrl[candidates], sizeof(relCandUrl[0]), url);
                    candidates++;
                }
                if (relSame(name, "SHA256SUMS")) relCopy(r->sums, sizeof(r->sums), url);
                a = jsNext(&j, ']');
            }
            if (a < 0) return 0;
        } else ok = jsSkip(&j, 1);
        if (!ok) return 0;
        more = jsNext(&j, '}');
    }
    if (more < 0) return 0;
    relCopy(r->version, sizeof(r->version), r->tag[0] == 'v' || r->tag[0] == 'V' ? r->tag + 1 : r->tag);
    if (!relIsVersion(r->version)) return 0;
    if (candidates) {
        char exact[200]; int pick = 0;
        relCopy(exact, sizeof(exact), prefix); relCat(exact, sizeof(exact), r->version); relCat(exact, sizeof(exact), suffix);
        for (int i = 0; i < candidates; i++) if (relSame(relCandName[i], exact)) { pick = i; break; }
        relCopy(r->asset, sizeof(r->asset), relCandUrl[pick]);
        relCopy(r->assetName, sizeof(r->assetName), relCandName[pick]);
    }
    if (!relStarts(r->page, REL_PAGES)) relCopy(r->page, sizeof(r->page), REL_PAGES); // only ever this repository's page
    relSummary(relNotes, r->summary, sizeof(r->summary));
    return 1;
}

// ---------------------------------------------------------------- the notes' summary

// The release notes in one line: the bold lead of each point of the first list
// ("- **Drag keys around:** on the ..." -> "Drag keys around"), else its first
// sentence, up to four, joined with " · ". Without a list: the first sentence.
static void relItem(const char *p, const char *end, char *out, unsigned cap) {
    char t[200]; unsigned k = 0; const char *start = p;
    int bold = p + 1 < end && p[0] == '*' && p[1] == '*';
    if (bold) p += 2;
    for (; p < end && k + 1 < sizeof(t); p++) {
        if (bold && p + 1 < end && p[0] == '*' && p[1] == '*') break;
        if (!bold && (*p == '.' || *p == '!' || *p == '?') && (p + 1 >= end || p[1] == ' ')) break;
        if (!bold && *p == ':' && k > 24) break;
        if (*p == '(' && p > start && p[-1] == ']') { while (p < end && *p != ')') p++; continue; } // [text](link) -> text
        if (*p == '*' || *p == '`' || *p == '[' || *p == ']' || *p == '_' || *p == '\r') continue;
        t[k++] = *p;
    }
    while (k && (t[k - 1] == ' ' || t[k - 1] == ':' || t[k - 1] == ',')) k--;
    t[k] = 0;
    if (k > 70) { // long: cut at a word (whole UTF-8 characters), with an ellipsis
        unsigned c = 70; while (c > 30 && t[c] != ' ') c--;
        while (c && ((unsigned char)t[c] & 0xC0) == 0x80) c--;
        t[c] = 0; relCat(t, sizeof(t), "\xE2\x80\xA6");
    }
    if (out[0]) relCat(out, cap, " \xC2\xB7 ");
    relCat(out, cap, t);
}
static void relSummary(const char *notes, char *out, unsigned cap) {
    out[0] = 0;
    int items = 0, inList = 0;
    const char *firstText = 0, *firstEnd = 0;
    for (const char *line = notes; *line && items < 4; ) {
        const char *end = line; while (*end && *end != '\n') end++;
        const char *p = line; while (p < end && (*p == ' ' || *p == '\t')) p++;
        int bullet = p + 1 < end && (*p == '-' || *p == '*' || *p == '+') && p[1] == ' ';
        if (bullet) { inList = 1; relItem(p + 2, end, out, cap); items++; }
        else if (inList && p < end && *p != '\r') break; // the first list is over
        else if (!firstText && p < end && *p != '#' && *p != '\r' && *p != '>') { firstText = p; firstEnd = end; }
        line = *end ? end + 1 : end;
    }
    if (!items && firstText) relItem(firstText, firstEnd, out, cap);
}

// ---------------------------------------------------------------- SHA-256

typedef struct { unsigned h[8], n; unsigned char block[64]; unsigned long long bits; } RELSHA;
static const unsigned RELK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
#define RELROR(x, k) ((x) >> (k) | (x) << (32 - (k)))
static void relShaBlock(RELSHA *c) {
    unsigned w[64], a[8];
    for (int i = 0; i < 16; i++) w[i] = (unsigned)c->block[4 * i] << 24 | (unsigned)c->block[4 * i + 1] << 16 | (unsigned)c->block[4 * i + 2] << 8 | c->block[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        unsigned s0 = RELROR(w[i - 15], 7) ^ RELROR(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = RELROR(w[i - 2], 17) ^ RELROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    for (int i = 0; i < 8; i++) a[i] = c->h[i];
    for (int i = 0; i < 64; i++) {
        unsigned t1 = a[7] + (RELROR(a[4], 6) ^ RELROR(a[4], 11) ^ RELROR(a[4], 25)) + ((a[4] & a[5]) ^ (~a[4] & a[6])) + RELK[i] + w[i];
        unsigned t2 = (RELROR(a[0], 2) ^ RELROR(a[0], 13) ^ RELROR(a[0], 22)) + ((a[0] & a[1]) ^ (a[0] & a[2]) ^ (a[1] & a[2]));
        a[7] = a[6]; a[6] = a[5]; a[5] = a[4]; a[4] = a[3] + t1; a[3] = a[2]; a[2] = a[1]; a[1] = a[0]; a[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) c->h[i] += a[i];
}
// The SHA-256 of d as 64 lowercase hex digits.
static void relSha256(const unsigned char *d, unsigned long long n, char hex[65]) {
    static const unsigned H0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    RELSHA c; c.n = 0; c.bits = n * 8;
    for (int i = 0; i < 8; i++) c.h[i] = H0[i];
    for (unsigned long long i = 0; i < n; i++) { c.block[c.n++] = d[i]; if (c.n == 64) { relShaBlock(&c); c.n = 0; } }
    c.block[c.n++] = 0x80;
    if (c.n > 56) { while (c.n < 64) c.block[c.n++] = 0; relShaBlock(&c); c.n = 0; }
    while (c.n < 56) c.block[c.n++] = 0;
    for (int i = 7; i >= 0; i--) c.block[c.n++] = (unsigned char)(c.bits >> (8 * i));
    relShaBlock(&c);
    for (int i = 0; i < 32; i++) { unsigned char b = (unsigned char)(c.h[i / 4] >> (24 - 8 * (i % 4))); hex[2 * i] = "0123456789abcdef"[b >> 4]; hex[2 * i + 1] = "0123456789abcdef"[b & 15]; }
    hex[64] = 0;
}

// The checksum SHA256SUMS lists for `name` ("<64 hex>  name" or "<64 hex> *name", as
// shasum writes it), in lowercase; 0 when it isn't there.
static int relSumFor(const char *sums, unsigned n, const char *name, char hex[65]) {
    unsigned m = relLen(name);
    for (unsigned i = 0; i < n; ) {
        unsigned end = i; while (end < n && sums[end] != '\n') end++;
        unsigned len = end - i; while (len && (sums[i + len - 1] == '\r' || sums[i + len - 1] == ' ')) len--;
        int ok = len > 66;
        for (unsigned k = 0; ok && k < 64; k++) {
            char c = sums[i + k];
            ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }
        if (ok && sums[i + 64] == ' ') {
            unsigned f = i + 65; if (f < i + len && (sums[f] == ' ' || sums[f] == '*')) f++;
            if (i + len - f == m) { unsigned k = 0; while (k < m && sums[f + k] == name[k]) k++; if (k == m) {
                for (k = 0; k < 64; k++) { char c = sums[i + k]; hex[k] = c >= 'A' && c <= 'F' ? (char)(c + 32) : c; }
                hex[64] = 0;
                return 1;
            } }
        }
        i = end + 1;
    }
    return 0;
}
