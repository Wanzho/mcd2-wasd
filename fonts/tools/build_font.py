# Adds the letters in glyphs.py to the heading font: glyphs.NEW are drawn from their
# pixel rows (an existing letter is redrawn in place), glyphs.SAME reuse an existing
# letter's glyph. Usage, from the repo folder:
#   python3 fonts/tools/build_font.py fonts/Mojang-Regular.ttf fonts/Mojang-Regular.ttf
# then put the font's base64 into configurator.html's @font-face.
import struct, sys
import ttf, glyphs
SRC, DST = sys.argv[1], sys.argv[2]
d = open(SRC, 'rb').read()
T = ttf.tables(d); cm = ttf.cmap(T)
ng = struct.unpack('>H', T['maxp'][4:6])[0]
head = T['head']; longloc = struct.unpack('>h', head[50:52])[0] == 1
loca = struct.unpack('>%d%s' % (ng + 1, 'I' if longloc else 'H'), T['loca'][:(ng + 1) * (4 if longloc else 2)])
if not longloc: loca = [x * 2 for x in loca]
nhm = struct.unpack('>H', T['hhea'][34:36])[0]
hm = [struct.unpack('>Hh', T['hmtx'][4*i:4*i + 4]) for i in range(nhm)]
hm += [(hm[-1][0], struct.unpack('>h', T['hmtx'][4*nhm + 2*(i - nhm):4*nhm + 2*(i - nhm) + 2])[0]) for i in range(nhm, ng)]
gdata = [T['glyf'][loca[i]:loca[i + 1]] for i in range(ng)]

def trace(top, rows):
    """Pixel rows -> clockwise contours (y up), in font units."""
    px = set()
    for r, line in enumerate(rows):
        y = top - r
        for x, ch in enumerate(line):
            if ch == '#': px.add((x, y))
    edges = {}
    def add(a, b): edges.setdefault(a, []).append(b)
    for (x, y) in px:
        if (x, y + 1) not in px: add((x, y + 1), (x + 1, y + 1))
        if (x + 1, y) not in px: add((x + 1, y + 1), (x + 1, y))
        if (x, y - 1) not in px: add((x + 1, y), (x, y))
        if (x - 1, y) not in px: add((x, y), (x, y + 1))
    contours = []
    while any(edges.values()):
        start = next(k for k, v in edges.items() if v)
        pts = [start]; cur = start; prev = None
        while True:
            outs = edges[cur]
            if len(outs) > 1 and prev is not None:
                # at a corner where two pixels touch diagonally, turn right (keeps loops simple)
                dx0, dy0 = cur[0] - prev[0], cur[1] - prev[1]
                def turn(n):
                    dx, dy = n[0] - cur[0], n[1] - cur[1]
                    return dx0 * dy - dy0 * dx  # >0 left, <0 right
                outs.sort(key=turn)
            nxt = outs.pop(0)
            prev, cur = cur, nxt
            if cur == start: break
            pts.append(cur)
        # drop points in the middle of straight runs
        simp = []
        for i, p in enumerate(pts):
            a, c = pts[i - 1], pts[(i + 1) % len(pts)]
            if (p[0] - a[0]) * (c[1] - p[1]) - (p[1] - a[1]) * (c[0] - p[0]) != 0: simp.append(p)
        contours.append([(x * ttf.PX, y * ttf.PX) for x, y in simp])
    width = max(len(l) for l in rows)
    return contours, width

def encode(contours):
    if not contours: return b''
    xs = [x for c in contours for x, y in c]; ys = [y for c in contours for x, y in c]
    out = struct.pack('>hhhhh', len(contours), min(xs), min(ys), max(xs), max(ys))
    ends, n = [], 0
    for c in contours: n += len(c); ends.append(n - 1)
    out += struct.pack('>%dH' % len(ends), *ends) + struct.pack('>H', 0)
    pts = [p for c in contours for p in c]
    out += bytes([1] * len(pts))  # on-curve, 16-bit deltas
    px = py = 0; xb = yb = b''
    for x, y in pts: xb += struct.pack('>h', x - px); px = x
    for x, y in pts: yb += struct.pack('>h', y - py); py = y
    return out + xb + yb

maxp_pts = struct.unpack('>H', T['maxp'][6:8])[0]; maxp_cont = struct.unpack('>H', T['maxp'][8:10])[0]
for ch, (top, rows) in glyphs.NEW.items():
    cp = ord(ch)
    contours, width = trace(top, rows)
    data = encode(contours)
    lsb = min(x for c in contours for x, y in c)
    gid = cm.get(cp)
    if gid is None:
        gid = len(gdata); gdata.append(data); hm.append(((width + 1) * ttf.PX, lsb))
    else:
        gdata[gid] = data; hm[gid] = ((width + 1) * ttf.PX, lsb)
    cm[cp] = gid
    maxp_pts = max(maxp_pts, sum(len(c) for c in contours)); maxp_cont = max(maxp_cont, len(contours))
for ch, src in glyphs.SAME.items():
    cm[ord(ch)] = cm[ord(src)]

# ---- write
ng = len(gdata)
glyf = b''; offs = []
for g in gdata:
    offs.append(len(glyf)); glyf += g + b'\0' * (-len(g) % 4)
offs.append(len(glyf))
T['glyf'] = glyf
T['loca'] = struct.pack('>%dI' % len(offs), *offs)
head = bytearray(T['head']); head[50:52] = struct.pack('>h', 1)
ymin = min(struct.unpack('>h', g[4:6])[0] for g in gdata if g); ymax = max(struct.unpack('>h', g[8:10])[0] for g in gdata if g)
xmax = max(struct.unpack('>h', g[6:8])[0] for g in gdata if g)
head[38:40] = struct.pack('>h', ymin); head[40:42] = struct.pack('>h', xmax); head[42:44] = struct.pack('>h', ymax)
T['head'] = bytes(head)
maxp = bytearray(T['maxp']); maxp[4:6] = struct.pack('>H', ng); maxp[6:8] = struct.pack('>H', maxp_pts); maxp[8:10] = struct.pack('>H', maxp_cont)
T['maxp'] = bytes(maxp)
T['hmtx'] = b''.join(struct.pack('>Hh', a, l) for a, l in hm)
hhea = bytearray(T['hhea']); hhea[34:36] = struct.pack('>H', ng)
hhea[10:12] = struct.pack('>H', max(a for a, l in hm)); T['hhea'] = bytes(hhea)
# cmap format 4: one segment per run of consecutive code points with consecutive glyphs
cps = sorted(c for c in cm if c <= 0xFFFF)
segs = []
for c in cps:
    if segs and c == segs[-1][1] + 1 and cm[c] - c == cm[segs[-1][0]] - segs[-1][0]: segs[-1][1] = c
    else: segs.append([c, c])
segs.append([0xFFFF, 0xFFFF])
n = len(segs); sr = 2 ** (n.bit_length() - 1) * 2
sub = struct.pack('>HHHHHHH', 4, 0, 0, n * 2, sr, sr.bit_length() - 2, n * 2 - sr)
sub += struct.pack('>%dH' % n, *[e for s, e in segs]) + b'\0\0' + struct.pack('>%dH' % n, *[s for s, e in segs])
sub += struct.pack('>%dh' % n, *[((cm[s] - s) if s != 0xFFFF else 1) & 0xFFFF if False else ((cm[s] - s + 0x8000) % 0x10000 - 0x8000 if s != 0xFFFF else 1) for s, e in segs])
sub += struct.pack('>%dH' % n, *([0] * n))
sub = sub[:2] + struct.pack('>H', len(sub)) + sub[4:]
T['cmap'] = struct.pack('>HH', 0, 2) + struct.pack('>HHI', 0, 3, 20) + struct.pack('>HHI', 3, 1, 20) + sub
os2 = bytearray(T['OS/2'])
r1 = struct.unpack('>I', os2[42:46])[0] | (1 << 1) | (1 << 2) | (1 << 9)  # Latin-1, Latin Extended-A, Cyrillic
os2[42:46] = struct.pack('>I', r1); os2[66:68] = struct.pack('>H', min(max(cps[:-0] or cps), 0xFFFF))
T['OS/2'] = bytes(os2)
# post stays format 3 (no glyph names)

def checksum(b):
    b = b + b'\0' * (-len(b) % 4); return sum(struct.unpack('>%dI' % (len(b) // 4), b)) & 0xFFFFFFFF
tags = sorted(T)
head = bytearray(T['head']); head[8:12] = b'\0\0\0\0'; T['head'] = bytes(head)
nt = len(tags); es = 2 ** (nt.bit_length() - 1)
out = struct.pack('>IHHHH', 0x00010000, nt, es * 16, es.bit_length() - 1, nt * 16 - es * 16)
off = 12 + 16 * nt; dirs = b''; body = b''
for t in tags:
    b = T[t]; dirs += struct.pack('>4sIII', t.encode('latin1'), checksum(b), off + len(body), len(b))
    body += b + b'\0' * (-len(b) % 4)
font = bytearray(out + dirs + body)
adj = (0xB1B0AFBA - checksum(bytes(font))) & 0xFFFFFFFF
hoff = 12 + 16 * nt + sum(len(T[t]) + (-len(T[t]) % 4) for t in tags[:tags.index('head')])
font[hoff + 8:hoff + 12] = struct.pack('>I', adj)
open(DST, 'wb').write(font)
print("glyphs", ng, "code points", len(cps))
