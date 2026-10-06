# Minimal TrueType reader/writer for a pixel font: every glyph is pixels of PX units.
import struct
PX = 128

def tables(d):
    n = struct.unpack('>H', d[4:6])[0]; T = {}
    for i in range(n):
        tag, cs, off, ln = struct.unpack('>4sIII', d[12 + 16*i:28 + 16*i]); T[tag.decode('latin1')] = d[off:off + ln]
    return T

def cmap(T):
    c = T['cmap']; out = {}
    nt = struct.unpack('>H', c[2:4])[0]
    for i in range(nt):
        pid, eid, so = struct.unpack('>HHI', c[4 + 8*i:12 + 8*i]); st = c[so:]
        if struct.unpack('>H', st[:2])[0] != 4: continue
        segx2 = struct.unpack('>H', st[6:8])[0]; seg = segx2 // 2
        ends = struct.unpack('>%dH' % seg, st[14:14 + segx2]); starts = struct.unpack('>%dH' % seg, st[16 + segx2:16 + 2*segx2])
        deltas = struct.unpack('>%dh' % seg, st[16 + 2*segx2:16 + 3*segx2])
        ro_at = 16 + 3*segx2; ros = struct.unpack('>%dH' % seg, st[ro_at:ro_at + segx2])
        for k, (s, e, dl, ro) in enumerate(zip(starts, ends, deltas, ros)):
            if s == 0xFFFF: continue
            for cp in range(s, e + 1):
                if ro == 0: g = (cp + dl) & 0xFFFF
                else:
                    p = ro_at + 2*k + ro + 2*(cp - s); g = struct.unpack('>H', st[p:p + 2])[0]
                    if g: g = (g + dl) & 0xFFFF
                out[cp] = g
    return out

def glyphs(T):
    head = T['head']; longloc = struct.unpack('>h', head[50:52])[0] == 1
    ng = struct.unpack('>H', T['maxp'][4:6])[0]
    loca = struct.unpack('>%d%s' % (ng + 1, 'I' if longloc else 'H'), T['loca'][:(ng + 1) * (4 if longloc else 2)])
    if not longloc: loca = [x * 2 for x in loca]
    nhm = struct.unpack('>H', T['hhea'][34:36])[0]
    hm = [struct.unpack('>Hh', T['hmtx'][4*i:4*i + 4]) for i in range(nhm)]
    out = []
    for g in range(ng):
        adv = hm[min(g, nhm - 1)][0]
        data = T['glyf'][loca[g]:loca[g + 1]]
        contours = []
        if data:
            nc = struct.unpack('>h', data[:2])[0]; assert nc >= 0, 'composite'
            ends = struct.unpack('>%dH' % nc, data[10:10 + 2*nc]); p = 10 + 2*nc
            il = struct.unpack('>H', data[p:p + 2])[0]; p += 2 + il
            npts = ends[-1] + 1 if nc else 0; flags = []
            while len(flags) < npts:
                f = data[p]; p += 1; flags.append(f)
                if f & 8: r = data[p]; p += 1; flags += [f] * r
            xs = []; v = 0
            for f in flags:
                if f & 2: dx = data[p]; p += 1; v += dx if f & 16 else -dx
                elif not f & 16: v += struct.unpack('>h', data[p:p + 2])[0]; p += 2
                xs.append(v)
            ys = []; v = 0
            for f in flags:
                if f & 4: dy = data[p]; p += 1; v += dy if f & 32 else -dy
                elif not f & 32: v += struct.unpack('>h', data[p:p + 2])[0]; p += 2
                ys.append(v)
            s = 0
            for e in ends: contours.append(list(zip(xs[s:e + 1], ys[s:e + 1]))); s = e + 1
        out.append((adv, contours))
    return out

def inside(x, y, contours):
    w = 0
    for c in contours:
        for i in range(len(c)):
            (x0, y0), (x1, y1) = c[i], c[(i + 1) % len(c)]
            if y0 <= y < y1 or y1 <= y < y0:
                xi = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                if xi > x: w += 1 if y1 > y0 else -1
    return w != 0

ROWS = list(range(7, -2, -1))  # pixel rows from y=7 (above cap) down to -2 (descender)
def bitmap(adv, contours):
    cols = adv // PX
    return [''.join('#' if inside(x*PX + 64, y*PX + 64, contours) else '.' for x in range(cols)) for y in ROWS]
